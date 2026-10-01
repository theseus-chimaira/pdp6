#include "pdp6.h"
#include <time.h>

/*
 * Stanford Phil Petit calendar clock, I/O device 0730.
 *
 * The original device exposes an unusual calendar encoding used by WAITS.
 * DAIMOS reads it directly, so retain the historical offsets and field
 * layout exactly.  Wall time is host UTC; the PDP-6 APR line clock remains
 * the independent monotonic/scheduler clock.
 *
 * Interrupt delivery is intentionally not implemented yet.  CONO still
 * records the low three PIA bits so adding the interrupt path later does not
 * require changing the guest-visible programming model.
 */

char *pclk_ident = PCLK_IDENT;

typedef struct Pclk Pclk;
struct Pclk
{
	Device dev;
	IOBus *bus;
	u8 pia;
};

static int
pclk_words(word *datai_word, word *coni_word)
{
	time_t now;
	struct tm *dt;
	unsigned int year;
	word year_month;

	now = time(nil);
	if(now == (time_t)-1)
		return 1;
	dt = gmtime(&now);
	if(dt == nil)
		return 1;

	year = (unsigned int)(dt->tm_year + 1900) % 100;
	year_month = ((word)(year / 10) << 8) |
	             ((word)(year % 10) << 4) |
	             (word)(dt->tm_mon + 4);

	*coni_word = ((word)dt->tm_min << 26) |
	             ((word)dt->tm_sec << 20);
	*coni_word = (*coni_word + 02020136700UL) & FW;

	*datai_word = (year_month << 16) |
	              ((word)(dt->tm_mday - 1) << 11) |
	              ((word)dt->tm_hour << 6) |
	              (word)dt->tm_min;
	*datai_word = (*datai_word + 05004UL) & FW;
	return 0;
}

static void
wake_pclk(void *dev)
{
	Pclk *pclk;
	IOBus *bus;
	word datai_word;
	word coni_word;

	pclk = dev;
	bus = pclk->bus;

	if(IOB_RESET)
		pclk->pia = 0;

	if(bus->devcode != PCLK)
		return;

	if(IOB_CONO_CLEAR)
		pclk->pia = 0;
	if(IOB_CONO_SET)
		pclk->pia |= bus->c12 & 07;

	if(!(IOB_STATUS || IOB_DATAI))
		return;
	if(pclk_words(&datai_word, &coni_word))
		return;

	if(IOB_STATUS)
		bus->c12 |= coni_word;
	if(IOB_DATAI)
		bus->c12 |= datai_word;
}

static void
pclkioconnect(Device *dev, IOBus *bus)
{
	Pclk *pclk;

	pclk = (Pclk*)dev;
	pclk->bus = bus;
	bus->dev[PCLK] = (Busdev){ pclk, wake_pclk, 0 };
}

Device*
makepclk(int argc, char *argv[])
{
	Pclk *pclk;

	(void)argc;
	(void)argv;
	pclk = malloc(sizeof(Pclk));
	memset(pclk, 0, sizeof(Pclk));
	pclk->dev.type = pclk_ident;
	pclk->dev.name = "";
	pclk->dev.ioconnect = pclkioconnect;
	return &pclk->dev;
}
