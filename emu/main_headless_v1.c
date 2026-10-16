/* Headless PDP-6: command interpreter, no panel or display transport. */
#include "pdp6.h"
#include "args.h"

/* These hooks belong to the graphical console, not the CPU. */
void
updatepanel(Apr *apr)
{
    (void)apr;
}

void
writeconsreg(Apr *apr, u32 addr, u32 data)
{
    (void)apr;
    (void)addr;
    (void)data;
}

u32
readconsreg(Apr *apr, u32 addr)
{
    (void)apr;
    (void)addr;
    return 0;
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-t] [-d debugfile]\n", argv0);
    exit(1);
}

int
threadmain(int argc, char *argv[])
{
    const char *outfile = "/dev/null";
    Channel *cmdchans[2];
    Task task;
    int cli_id;

    ARGBEGIN {
    case 't':
        dotrace = 1;
        break;
    case 'd':
        outfile = EARGF(usage());
        break;
    default:
        usage();
    } ARGEND;

    debugfp = fopen(outfile, "w");
    if (debugfp == nil)
        err("cannot open debug file %s", outfile);

    rtcchan = chancreate(sizeof(RtcMsg), 20);
    /* The graphical init.ini mounts host-specific panel and tape devices.
     * Start from the self-contained CPU/I/O configuration instead; users
     * may mount media with the normal command interpreter afterwards. */
    defaultconfig();
    if (getdevice("apr") == nil)
        err("need APR device");

    cmdchans[0] = chancreate(sizeof(char *), 1);
    cmdchans[1] = chancreate(sizeof(void *), 1);
    task = (Task){ nil, readcmdchan, cmdchans, 10, 0 };
    addtask(task);

    threadcreate(simthread, nil);
    threadcreate(rtcthread, nil);
    cli_id = threadcreate(cmdthread, cmdchans);
    threadwait(cli_id);
    return 0;
}
