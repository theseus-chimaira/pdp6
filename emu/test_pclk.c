#include "pdp6.h"
#include <assert.h>
#include <time.h>

static IOBus iobus;
static IOBus *bus = &iobus;

void
setreq(IOBus *b, int dev, u8 pia)
{
	(void)b;
	(void)dev;
	(void)pia;
}

static void
wakedev(int d)
{
	Busdev *dev;

	bus->devcode = d;
	dev = &bus->dev[d];
	if(dev->wake)
		dev->wake(dev->dev);
}

static word
coni(int d)
{
	word w;

	bus->c34 |= IOBUS_IOB_STATUS;
	wakedev(d);
	bus->c34 &= ~IOBUS_IOB_STATUS;
	w = bus->c12;
	bus->c12 = 0;
	return w;
}

static word
datai(int d)
{
	word w;

	bus->c34 |= IOBUS_IOB_DATAI;
	wakedev(d);
	bus->c34 &= ~IOBUS_IOB_DATAI;
	w = bus->c12;
	bus->c12 = 0;
	return w;
}

static int
bcd(unsigned int n)
{
	return ((n / 10) << 4) | (n % 10);
}

int
main(void)
{
	Device *dev;
	time_t before;
	time_t after;
	struct tm *dt;
	word di;
	word ci;
	word raw_di;
	word raw_ci;
	unsigned int year_month;
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;

	dev = makepclk(0, nil);
	dev->ioconnect(dev, bus);

	before = time(nil);
	raw_di = datai(PCLK);
	raw_ci = coni(PCLK);
	after = time(nil);
	assert(before != (time_t)-1);
	assert(after != (time_t)-1);
	assert(after - before <= 1);

	di = (raw_di - 05004UL) & FW;
	ci = (raw_ci - 02020136700UL) & FW;
	year_month = (unsigned int)((di >> 16) & 07777);
	year = (year_month >> 8) * 10 + ((year_month >> 4) & 017);
	month = (year_month & 017) - 3;
	day = (unsigned int)((di >> 11) & 037) + 1;
	hour = (unsigned int)((di >> 6) & 037);
	minute = (unsigned int)(di & 077);
	second = (unsigned int)((ci >> 20) & 077);

	dt = gmtime(&after);
	assert(dt != nil);
	assert((unsigned int)bcd((unsigned int)(dt->tm_year + 1900) % 100) ==
	       (unsigned int)((year_month >> 4) & 0377));
	assert(year == (unsigned int)(dt->tm_year + 1900) % 100);
	assert(month == (unsigned int)dt->tm_mon + 1);
	assert(day == (unsigned int)dt->tm_mday);
	assert(hour == (unsigned int)dt->tm_hour);
	assert(minute == (unsigned int)dt->tm_min);
	assert(second == (unsigned int)dt->tm_sec ||
	       (second == 59 && dt->tm_sec == 0) ||
	       (second + 1 == (unsigned int)dt->tm_sec));
	free(dev);
	return 0;
}
