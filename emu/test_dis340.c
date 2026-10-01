/*
 * Focused Type 340/344 interface regression tests.
 *
 * Include the implementation so this test can exercise the display's internal
 * state machine without making emulator-only internals part of pdp6.h.
 */
#include "dis340.c"

/* Only the interface-level helpers exercised below survive --gc-sections. */
void
lock(Lock *lk)
{
	(void)lk;
}

void
unlock(Lock *lk)
{
	(void)lk;
}

void
setreq2(IOBus *bus, int dev, u8 req)
{
	bus->dev[dev].req = req;
}

static int
check_status_layout(void)
{
	Dis340 dis;
	IOBus bus;
	word want;

	memset(&dis, 0, sizeof(dis));
	memset(&bus, 0, sizeof(bus));
	dis.bus = &bus;
	dis.mode = VCM;
	dis.vef = 1;
	dis.lp_flag = 1;
	dis.hef = 1;
	dis.stop = 1;
	dis.br = 01000;
	dis.rfd = 1;
	bus.devcode = DIS;
	bus.c34 = IOBUS_IOB_STATUS;

	wake_dis(&dis);
	want = F0 | ((word)VCM << 15) | F24 | F25 | F26 | F27 | F28;
	if((bus.c12 & want) != want){
		fprintf(stderr, "status layout: got %012lo want mask %012lo\n",
		    bus.c12, want);
		return 1;
	}
	return 0;
}

static int
check_resume(void)
{
	Dis340 dis;
	IOBus bus;

	memset(&dis, 0, sizeof(dis));
	memset(&bus, 0, sizeof(bus));
	dis.bus = &bus;
	dis.mode = VM;
	dis.lp_enable = 1;
	dis.lp_find = 1;
	dis.lp_flag = 1;
	dis.hef = 1;
	dis.vef = 1;
	bus.devcode = DIS;
	bus.c12 = F28;
	bus.c34 = IOBUS_CONO_SET;

	wake_dis(&dis);
	if(dis.lp_flag || dis.lp_find || dis.hef || dis.vef){
		fprintf(stderr, "resume did not clear special stop state\n");
		return 1;
	}
	if(!dis.lp_enable){
		fprintf(stderr, "resume incorrectly disabled the light pen\n");
		return 1;
	}
	if(!dis.intdelay){
		fprintf(stderr, "resume did not restart a paused vector sequence\n");
		return 1;
	}
	return 0;
}

static int
check_lightpen_latch(void)
{
	Dis340 dis;
	IOBus bus;
	static CRT crt;

	memset(&dis, 0, sizeof(dis));
	memset(&bus, 0, sizeof(bus));
	memset(&crt, 0, sizeof(crt));
	dis.bus = &bus;
	dis.crt = &crt;
	dis.lp_enable = 1;
	dis.pen = 1;
	dis.penx = 0123;
	dis.peny = 0456;
	dis.x = 0123;
	dis.y = 0456;
	dis.i = 7;

	intensify(&dis);
	if(!dis.lp_find || dis.lpx != 0123 || dis.lpy != 0456){
		fprintf(stderr, "light pen hit was not latched at beam coordinates\n");
		return 1;
	}

	dis.lp_flag = 1;
	dis.x = 0777;
	dis.y = 0111;
	bus.devcode = DIS;
	bus.c34 = IOBUS_IOB_DATAI;
	wake_dis(&dis);
	if(bus.c12 != (((word)0456 << 18) | 0123)){
		fprintf(stderr, "DATAI returned %012lo, expected latched Y,,X\n",
		    bus.c12);
		return 1;
	}
	return 0;
}

static int
check_no_ink(void)
{
	Dis340 dis;
	IOBus bus;
	static CRT crt;

	memset(&dis, 0, sizeof(dis));
	memset(&bus, 0, sizeof(bus));
	memset(&crt, 0, sizeof(crt));
	dis.bus = &bus;
	dis.crt = &crt;
	dis.i = 7;
	dis.x = 010;
	dis.y = 020;
	bus.devcode = DIS;

	bus.c12 = F25;
	bus.c34 = IOBUS_CONO_SET;
	wake_dis(&dis);
	if(!dis.no_ink){
		fprintf(stderr, "CONO SET NO INK was ignored\n");
		return 1;
	}
	intensify(&dis);
	if(crt.numPixels != 0){
		fprintf(stderr, "NO INK still intensified the CRT\n");
		return 1;
	}

	bus.c12 = F24;
	bus.c34 = IOBUS_CONO_SET;
	wake_dis(&dis);
	if(dis.no_ink){
		fprintf(stderr, "CONO CLEAR NO INK was ignored\n");
		return 1;
	}
	intensify(&dis);
	if(crt.numPixels != 1){
		fprintf(stderr, "clearing NO INK did not restore intensification\n");
		return 1;
	}
	return 0;
}

int
main(void)
{
	int failed;

	failed = 0;
	failed |= check_status_layout();
	failed |= check_resume();
	failed |= check_lightpen_latch();
	failed |= check_no_ink();
	if(failed)
		return 1;
	puts("test_dis340: PASS");
	return 0;
}
