#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "JsonHelper.h"

#include "ClockifyManager.h"
#include "ClockifyUser.h"
#include "Common.h"

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	QCommandLineParser parser;
	QCommandLineOption on{"on"};
	QCommandLineOption off{"off"};
	QCommandLineOption breakTime{"break"};
	QCommandLineOption endBreak{"end-break"};
	QCommandLineOption running{"running"};
	QCommandLineOption working{"working"};
	parser.addOption(on);
	parser.addOption(off);
	parser.addOption(breakTime);
	parser.addOption(endBreak);
	parser.addOption(running);
	parser.addOption(working);
	parser.process(a);

	ClockifyManager manager{WORKSPACE, APIKEY, &a};
	if (!manager.isValid())
		return -1;

	ClockifyUser me{"redacted", &manager};

	auto x = me.getRunningTimeEntry();

	if (parser.isSet(on) && !me.hasRunningTimeEntry())
		me.startTimeEntry(WORKTIME);
	else if (parser.isSet(off) && me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
	else if (parser.isSet(breakTime))
	{
		if (me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
		me.startTimeEntry(BREAKTIME);
	}
	else if (parser.isSet(endBreak))
	{
		if (me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
		me.startTimeEntry(WORKTIME);
	}
	else if (parser.isSet(running))
		return !me.hasRunningTimeEntry();
	else if (parser.isSet(working))
	{
		if (!me.hasRunningTimeEntry())
			return 1;
		else
			return me.getRunningTimeEntry()["projectId"].get<QString>() == BREAKTIME; // get first (and only) json entry
	}

	return 0;
}
