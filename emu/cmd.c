#include "pdp6.h"
#include "args.h"

#define MAXOPS 20

static char *lp;
static int numops;
static char *ops[MAXOPS];

/* current apr */
static Apr *apr;

static Apr*
getapr(void)
{
	Device *dev;

	if(apr) return apr;

	for(dev = devlist; dev; dev = dev->next)
		if(dev->type == apr_ident){
			apr = (Apr*)dev;
			goto found;
		}
	err("No APR defined");
found:
	return apr;
}

static void
skipwhite(void)
{
	while(isspace(*lp))
		lp++;
}

static int
isdelim(char c)
{
	return c == '\0' || strchr(" \t\n;'\"", c) != nil;
}

/* tokenize lp into ops[numops] */
static void
splitops(void)
{
	char delim;
	char *p;

	numops = 0;
	for(; *lp; lp++){
		skipwhite();
		if(*lp == ';' || *lp == '\0')
			return;
		if(numops == MAXOPS){
			fprintf(stderr, "Too many arguments, ignored <%s>\n", lp);
			return;
		}
		if(*lp == '"' || *lp == '\''){
			delim = *lp++;
			ops[numops++] = p = lp;
			while(*lp && *lp != delim){
				if(*lp == '\\')
					lp++;
				*p++ = *lp++;
			}
			if(*lp == '\0') lp--;
			*p = '\0';
		}else{
			ops[numops++] = p = lp;
			while(!isdelim(*lp)){
				if(*lp == '\\')
					lp++;
				*p++ = *lp++;
			}
			if(*lp == '\0') lp--;
			*p = '\0';
		}
	}
}

/* Parse an octal, decimal or hexadecimal number.
 *  0nnnn  is an octal number.
 *   nnnn. is a decimal number.
 * 0xnnnn  is a hexadecimal number.
 * returns ~0 on error.
 */
word
parsen(char **sp)
{
	char *s;
	word dec, oct, hex;
	char c;
	int base;

	s = *sp;
	base = 0;
	dec = oct = hex = 0;
	if(s[0] == '0'){
		s++;
		base = 8;
		if(*s == 'x' || *s == 'X'){
			s++;
			base = 16;
		}
	}
	while(c = *s++){
		if(c >= '0' && c <= '9')
			c -= '0';
		else if(c >= 'A' && c <= 'F')
			c -= 'A' - 10;
		else if(c >= 'a' && c <= 'f')
			c -= 'a' - 10;
		else if(c == '.'){
			/* decimal number, but only if . is at the end */
			if(base <= 10 && *s == '\0'){
				base = 10;
				break;
			}else
				goto fail;
		}else
			break;
		oct = oct*8 + c;
		dec = dec*10 + c;
		hex = hex*16 + c;
		if(c >= 8){
			if(base < 8)
				base = 8;
			else
				goto fail;
		}
		if(c >= 10){
			if(base < 10)
				base = 10;
			else
				goto fail;
		}
		if(c >= 16){
			if(base < 16)
				base = 16;
			else
				goto fail;
		}
	}
	*sp = s-1;
	if(base == 16) return hex;
	if(base == 10) return dec;
	return oct;
fail:
	printf("error: number format\n");
	return ~0;
}

static int
parserange(char *s, word *start, word *end)
{
	*start = *end = ~0;

	*start = parsen(&s);
	if(*start == ~0) return 1;

	if(*s == '\0'){
		*end = *start;
		return 0;
	}else if(*s == '-' || *s == ':'){
		s++;
		*end = parsen(&s);
		if(*end == ~0) return 1;
	}
	if(*s){
		*start = *end = ~0;
		printf("error: address format\n");
		return 1;
	}
	return 0;
}

enum
{
	FmtOct, FmtMnem
};

static void
exam(word addr, int fastmem, int format)
{
	word *p;
	p = getmemref(&getapr()->membus, addr, fastmem, nil);
	if(p == nil){
		printf("Non existent memory\n");
		return;
	}
	printf("%06lo: ", addr);
	switch(format){
	case FmtOct:
	default:
		printf("%012lo\n", *p);
		break;

	case FmtMnem:
		printf("%s\n", disasm(*p));
		break;
	}
}

static void
dep(word addr, int fastmem, word w)
{
	word *p;
	p = getmemref(&getapr()->membus, addr, fastmem, nil);
	if(p == nil){
		printf("Non existent memory\n");
		return;
	}
	*p = w;
}

/* Commands */

typedef struct DevDef DevDef;
struct DevDef
{
	char *type;
	Device *(*make)(int argc, char *argv[]);
};
DevDef definitions[] = {
	{ FMEM_IDENT, makefmem },
	{ CMEM_IDENT, make16kmem },
	{ MMEM_IDENT, make256kmem },
	{ APR_IDENT, makeapr },
	{ TTY_IDENT, maketty },
	{ PTR_IDENT, makeptr },
	{ PTP_IDENT, makeptp },
	{ DC_IDENT, makedc },
	{ DT_IDENT, makedt },
	{ DX_IDENT, makedx },
	{ PCLK_IDENT, makepclk },
#ifdef GRAPHICS
	{ DIS_IDENT, makedis },
	{ JOY_IDENT, makejoy },
	{ OJOY_IDENT, makeojoy },
#endif
	{ NETMEM_IDENT, makenetmem },
	{ NETCONS_IDENT, makenetcons },
	{ nil, nil }
};

static void
c_mkdev(int argc, char *argv[])
{
	Device *dev;
	DevDef *def;

	if(argc < 3){
		printf("Not enough arguments\n");
		return;
	}
	for(def = definitions; def->type; def++)
		if(strcmp(def->type, argv[2]) == 0){
			dev = def->make(argc-3, argv+3);
			goto found;
		}
	printf("No such device type: %s\n", argv[2]);
	return;
found:
	dev->name = strdup(argv[1]);
	adddevice(dev);
}

static void
c_mount(int argc, char *argv[])
{
	Device *dev;

	if(argc < 3){
		printf("Not enough arguments\n");
		return;
	}
	dev = getdevice(argv[1]);
	if(dev == nil){
		printf("No device: %s\n", argv[1]);
		return;
	}
	if(dev->mount == nil){
		printf("No mount for device type %s\n", dev->type);
		return;
	}
	dev->mount(dev, argv[2]);
}

static void
c_unmount(int argc, char *argv[])
{
	Device *dev;

	if(argc < 2){
		printf("Not enough arguments\n");
		return;
	}
	dev = getdevice(argv[1]);
	if(dev == nil){
		printf("No device: %s\n", argv[1]);
		return;
	}
	if(dev->unmount == nil){
		printf("No unmount for device type %s\n", dev->type);
		return;
	}
	dev->unmount(dev);
}

/* ioconnect dev busdev */
static void
c_ioconnect(int argc, char *argv[])
{
	Device *dev, *busdev;

	if(argc < 3){
		printf("Not enough arguments\n");
		return;
	}
	dev = getdevice(argv[1]);
	if(dev == nil){
		printf("No device: %s\n", argv[1]);
		return;
	}
	busdev = getdevice(argv[2]);
	if(busdev == nil){
		printf("No device: %s\n", argv[2]);
		return;
	}
	if(busdev->type != apr_ident){
		printf("No bus device: %s\n", busdev->type);
		return;
	}
	if(dev->ioconnect == nil){
		printf("No ioconnect for device type %s\n", dev->type);
		return;
	}
	dev->ioconnect(dev, &((Apr*)busdev)->iobus);
}

/* memconnect dev proc busdev addr */
static void
c_memconnect(int argc, char *argv[])
{
	int proc, addr;
	Device *dev, *busdev;

	if(argc < 5){
		printf("Not enough arguments\n");
		return;
	}
	dev = getdevice(argv[1]);
	if(dev == nil){
		printf("No device: %s\n", argv[1]);
		return;
	}
	proc = atoi(argv[2]);
	busdev = getdevice(argv[3]);
	if(busdev == nil){
		printf("No device: %s\n", argv[3]);
		return;
	}
	if(busdev->type != apr_ident){
		printf("No bus device: %s\n", argv[3]);
		return;
	}
	addr = strtol(argv[4], nil, 8);
	attachmem((Mem*)dev, proc, &((Apr*)busdev)->membus, addr);
}

/* devconnect dev1 dev2 */
// TODO: not quite sure this is the right way to do it
static void
c_devconnect(int argc, char *argv[])
{
	Device *dev1, *dev2;

	if(argc < 3){
args:
		printf("Not enough arguments\n");
		return;
	}
	dev1 = getdevice(argv[1]);
	if(dev1 == nil){
		printf("No device: %s\n", argv[1]);
		return;
	}
	dev2 = getdevice(argv[2]);
	if(dev2 == nil){
		printf("No device: %s\n", argv[2]);
		return;
	}
	if(dev1->type == dc_ident && dev2->type == dt_ident)
		dtconn((Dc136*)dev1, (Dt551*)dev2);
	else if(dev1->type == dt_ident && dev2->type == dx_ident){
		int n;
		if(argc < 4)
			goto args;
		n = atoi(argv[3]);
		if(n < 1 || n > 8){
			printf("Invalid device number %d\n", n);
			return;
		}
		dxconn((Dt551*)dev1, (Dx555*)dev2, n);
	}else
		printf("Cannot connect %s to %s\n", dev2->type, dev1->type);
}

static void
c_ex(int argc, char *argv[])
{
	char *argv0 = nil;
	word start, end;
	int format, fastmem;

	format = FmtOct;
	fastmem = 1;
	ARGBEGIN{
	case 's':	/* shadow mode */
		fastmem = 0;
		break;
	case 'm':
		format = FmtMnem;
		break;
	}ARGEND;
	if(argc < 1)
		return;
	if(parserange(argv[0], &start, &end))
		return;
	for(; start <= end; start++)
		exam(start, fastmem, format);
}

static void
c_dep(int argc, char *argv[])
{
	char *argv0 = nil;
	word start, end;
	word w;
	int fastmem;

	fastmem = 1;
	ARGBEGIN{
	case 's':
		fastmem = 0;
		break;
	}ARGEND;
	if(argc < 2)
		return;
	if(parserange(argv[0], &start, &end))
		return;
	w = parsen(&argv[1]);
	if(w == ~0)
		return;
	for(; start <= end; start++)
		dep(start, fastmem, w);
}

/* parse "devname.reg".
 * look up "devname" and return it.
 * also remove "devname." from original string.
 * if "devname." is missing, return last device.
 * return apr if anything goes wrong */
Device*
getexdepdev(char *dev)
{
	static Device *exdepdev;
	char *s;
	Device *d;

	s = strchr(dev, '.');
	if(s == nil){
		if(exdepdev == nil)
			exdepdev = &getapr()->dev;
		return exdepdev;
	}
	*s++ = '\0';
	d = getdevice(dev);
	exdepdev = d ? d : &getapr()->dev;
	while(*dev++ = *s++);
	return exdepdev;
}

static void
c_xex(int argc, char *argv[])
{
	int i;
	Device *dev;
	word w;
	for(i = 1; i < argc; i++){
		dev = getexdepdev(argv[i]);
		if(dev->examine){
			strtolower(argv[i]);
			w = dev->examine(dev, argv[i]);
			if(w == ~0)
				printf("can't examine %s\n", argv[i]);
			else
				printf("%012lo\n", w);
		}else
			printf("no examine on %s\n", dev->type);
	}
}

static void
c_xdep(int argc, char *argv[])
{
	int i;
	Device *dev;
	word w;

	if(argc < 3)
		return;

	w = parsen(&argv[argc-1]);
	if(w == ~0)
		return;

	for(i = 1; i < argc-1; i++){
		dev = getexdepdev(argv[i]);
		if(dev->deposit){
			strtolower(argv[i]);
			if(dev->deposit(dev, argv[i], w))
				printf("can't deposit %s\n", argv[i]);
		}else
			printf("no deposit on %s\n", dev->type);
	}

void updatepanel(Apr *apr);
	updatepanel(getapr());
}

enum
{
	FmtUnk,
	FmtSav,
	FmtSblk,
	FmtRim10
};

static void
loadsav(FILE *fp)
{
	word iowd;
	word w;
	while(w = readwbak(fp), w != ~0){
		if(w >> 27 == 0254){
			getapr()->pc = right(w);
			return;
		}
		iowd = w;
		while(left(iowd) != 0){
			iowd += 01000001;
			w = readwbak(fp);
			if(w == ~0)
				goto format;
			dep(right(iowd), 0, w);
		}
	}
format:
	printf("SAV format botch\n");
}

static void
loadsblk(FILE *fp)
{
	word iowd, w, chk;
	int d;

	/* Skip RIM loader */
	while(w = readwits(fp), w != ~0)
		if(w == 0254000000001)
			goto sblk;
	goto format;
sblk:
	/* Read a simple block */
	while(w = readwits(fp), w != ~0){
		/* We expect an AOBJN word here */
		if((w & F0) == 0)
			goto end;
		iowd = w;
		chk = iowd;
		d = right(iowd) != 0;	/* 0 is symbol table */
		while(left(iowd) != 0){
			w = readwits(fp);
			if(w == ~0)
				goto format;

			chk = (chk<<1 | chk>>35) + w & FW;

			if(d)
				dep(right(iowd), 0, w);
			iowd += 01000001;
		}
		if(readwits(fp) != chk)
			goto format;
	}
	goto format;
end:
	/* use the first JRST, not the second */
//	w = readwits(fp);
	if(left(w) != 0324000 && left(w) != 0254000)
		goto format;
	getapr()->pc = right(w);
	return;

format:
	printf("SBLK format botch\n");
}

static void
loadrim10(FILE *fp)
{
	word iowd, w, chk;
	int nldr;
	int mac;

	w = readw(fp);
	nldr = 0;
	while(left(w) != 0){
		nldr++;
		readw(fp);
		w += 01000001;
	}
	if(nldr == 017)
		mac = 1;
	else if(nldr == 016)
		mac = 0;
	else
		goto format;

	/* Read a simple block */
	while(w = readw(fp), w != ~0){
		/* We expect an AOBJN word here */
		if((w & F0) == 0)
			goto end;
		iowd = w;
		chk = iowd;
		while(left(iowd) != 0){
			w = readw(fp);
			if(w == ~0)
				goto format;

			if(mac){
				chk = (chk<<1 | chk>>35) + w & FW;
				dep(right(iowd), 0, w);
			}else{
				chk = chk + w & FW;
				dep(right(iowd+1), 0, w);
			}
			iowd += 01000001;
		}
		if(readw(fp) != chk)
			goto format;
	}
	goto format;
end:
	/* JRST */
	if(left(w) != 0324000 && left(w) != 0254000)
		goto format;
	getapr()->pc = right(w);
	return;

format:
	printf("RIM10 format botch\n");
}

static void
c_load(int argc, char *argv[])
{
	char *argv0 = nil;
	FILE *fp;
	int fmt;

	fmt = FmtSav;	// assume SAV for now
	ARGBEGIN{
	case 's':
		fmt = FmtSav;
		break;
	case 'b':
		fmt = FmtSblk;
		break;
	case 'r':
		fmt = FmtRim10;
		break;
	}ARGEND;
	if(argc < 1)
		return;

	fp = fopen(argv[0], "rb");
	if(fp == nil){
		printf("couldn't open file: %s\n", argv[0]);
		return;
	}
	switch(fmt){
	case FmtSav: loadsav(fp); break;
	case FmtSblk: loadsblk(fp); break;
	case FmtRim10: loadrim10(fp); break;
	}
	fclose(fp);
}

static void
c_show(int argc, char *argv[])
{
	printf("Devices:\n");
	showdevices();
	printf("Memory:\n");
	showmem(&getapr()->membus);
}

static void
c_readin(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	apr_readin(getapr());
}

static void
c_start(int argc, char *argv[])
{
	word addr;

	if(argc < 2){
		printf("Not enough arguments\n");
		return;
	}
	addr = parsen(&argv[1]);
	if(addr == ~0 || addr > RT){
		printf("Invalid start address\n");
		return;
	}
	apr_start_at(getapr(), (hword)addr);
}

/* The command thread dispatches commands in the simulation thread.
 * Inspect RUN there, never by unsynchronized polling in the CLI thread.
 * A queued START must not be mistaken for a completed HALT. */
static void
c_cpustatus(int argc, char *argv[])
{
	Apr *a = getapr();
	const char *state;
	(void)argc;
	(void)argv;
	if(a->run)
		state = "RUNNING";
	else if(a->remote_start || a->key_start ||
	        (a->extpulse & EXT_KEY_START))
		state = "START_PENDING";
	else
		state = "HALTED";
	printf("APR_STATUS_V1 %s\n", state);
	fflush(stdout);
}

/* Read-only APR arithmetic diagnostic. The command dispatcher runs on
 * the simulation thread, so flag values form a coherent snapshot. */
static void
c_aprflags(int argc, char *argv[])
{
	Apr *a = getapr();
	(void)argc;
	(void)argv;
	printf("APR_FLAGS_V1 OV=%d CRY0=%d CRY1=%d PCCHG=%d\n",
	       !!a->ar_ov_flag, !!a->ar_cry0_flag,
	       !!a->ar_cry1_flag, !!a->ar_pc_chg_flag);
	fflush(stdout);
}

/* Snapshot architectural exceptional-state latches at a console
 * command boundary. Report facts, not a guessed instruction outcome. */
static void
c_aprclass(int argc, char *argv[])
{
	Apr *a = getapr();
	(void)argc;
	(void)argv;
	printf("APR_CLASS_V1 NXM=%d ILLOP=%d PDLOV=%d PI_ACTIVE=%d PI_REQ=%03o PI_CYC=%d\n",
	       !!a->cpa_non_exist_mem, !!a->cpa_illeg_op,
	       !!a->cpa_pdl_ov, !!a->pi_active,
	       a->pi_req, !!a->pi_cyc);
	fflush(stdout);
}

static void
c_switches(int argc, char *argv[])
{
	word switches;

	if(argc < 2){
		printf("Not enough arguments\n");
		return;
	}
	switches = parsen(&argv[1]);
	if(switches == ~0)
		return;
	apr_set_switches(getapr(), switches);
}

static void
c_power(int argc, char *argv[])
{
	if(argc < 2){
		printf("Not enough arguments\n");
		return;
	}
	if(strcmp(argv[1], "on") == 0)
		apr_set_power(getapr(), 1);
	else if(strcmp(argv[1], "off") == 0)
		apr_set_power(getapr(), 0);
	else
		printf("power expects on or off\n");
}

static void
c_ptrmotor(int argc, char *argv[])
{
	Device *dev;
	int on;

	if(argc < 3){
		printf("Not enough arguments\n");
		return;
	}
	dev = getdevice(argv[1]);
	if(dev == nil || strcmp(dev->type, ptr_ident) != 0){
		printf("%s is not a Type 760 paper tape reader\n", argv[1]);
		return;
	}
	if(strcmp(argv[2], "on") == 0)
		on = 1;
	else if(strcmp(argv[2], "off") == 0)
		on = 0;
	else{
		printf("ptrmotor expects on or off\n");
		return;
	}
	ptr_setmotor((Ptr*)dev, on);
}

static void
c_quit(int argc, char *argv[])
{
	quit(0);
}

static void c_help(int argc, char *argv[]);

struct {
	char *cmd;
	void (*f)(int, char **);
	char *desc;
} cmdtab[] = {
	{ "mkdev", c_mkdev,
		"make a device: mkdev name type [args]" },
	{ "mount", c_mount,
		"mount a file on a device: mount name filename" },
	{ "unmount", c_unmount,
		"unmount a file from a device: unmount name" },
	{ "connectio", c_ioconnect,
		"connect device to IO bus: connectio devname procname" },
	{ "connectmem", c_memconnect,
		"connect memory to mem bus: connectmem memname memport procname address/16k" },
	{ "connectdev", c_devconnect,
		"connect devices: connectdev dev1name dev2name [args]" },
	{ "examine", c_ex,
		"examine memory: examine [-sm] memrange" },
	{ "deposit", c_dep,
		"deposit memory: deposit [-s] memrange word" },
	{ "xexamine", c_xex,
		"examine device" },
	{ "xdeposit", c_xdep,
		"deposit device" },
	{ "load", c_load,
		"load file into memory: load [-sbr] filename" },
	{ "show", c_show,
		"show configuration" },
	{ "readin", c_readin,
		"press the PDP-6 READ IN key" },
	{ "start", c_start,
		"press START with an automated address: start address" },
	{ "cpustatus", c_cpustatus,
		"report APR_STATUS_V1 START_PENDING, RUNNING, or HALTED" },
	{ "aprflags", c_aprflags,
		"report APR_FLAGS_V1 arithmetic and PC-change flip-flops" },
	{ "aprclass", c_aprclass,
		"report APR_CLASS_V1 memory fault, illegal-op and PI latches" },
	{ "switches", c_switches,
		"set the automated PDP-6 console switch word" },
	{ "power", c_power,
		"set automated PDP-6 panel power: power on|off" },
	{ "ptrmotor", c_ptrmotor,
		"set Type 760 reader motor: ptrmotor device on|off" },
	{ "quit", c_quit,
		"quit emulator" },
	{ "help", c_help,
		"print help" },
	{ nil, nil}
};

static void
c_help(int argc, char *argv[])
{
	int i;

	for(i = 0; cmdtab[i].cmd; i++)
		printf(" %s:\t%s\n", cmdtab[i].cmd, cmdtab[i].desc);
}

void
commandline(char *line)
{
	int i, n;
	int nfound;
	size_t l;
	char *cmd;

	lp = line;
	splitops();

	if(numops && ops[0][0] != '#'){
		nfound = 0;
		for(i = 0; cmdtab[i].cmd; i++){
			cmd = cmdtab[i].cmd;
			l = strlen(ops[0]);
			if(strncmp(ops[0], cmd, l) == 0){
				n = i;
				nfound++;
			}
		}
		if(nfound == 0)
			printf("%s: not found\n", ops[0]);
		else if(nfound == 1)
			cmdtab[n].f(numops, ops);
		else{
			printf("Ambiguous command: %s\n", ops[0]);
			for(i = 0; cmdtab[i].cmd; i++){
				cmd = cmdtab[i].cmd;
				l = strlen(ops[0]);
				if(strncmp(ops[0], cmd, l) == 0)
					printf("	%s\n", cmd);
			}
		}
	}
}

/* If there is no channel, execute immediately.
 * Otherwise send line through channel so it can
 * be executed synchronously in the simulation thread. */
void
cli(FILE *in, Channel **c)
{
	size_t len;
	char *line;

	apr = nil;
	for(;;){
		line = nil;
		len = 0;
		if(in == stdin)
			printf("> ");
		if(getline(&line, &len, in) < 0)
			return;
		if(c){
			chansend(c[0], &line);
			chanrecv(c[1], &line);	// wait for completion
		}else{
			commandline(line);
			free(line);
		}
	}
}

void*
cmdthread(void *p)
{
	threadname("cli");
	cli(stdin, p);
	quit(0);
	return nil;
}
