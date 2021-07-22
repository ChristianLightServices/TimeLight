#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "JsonHelper.h"

#include "ClockifyManager.h"
#include "ClockifyUser.h"

const QByteArray WORKSPACE{"5e207622ba05e2483903da0d"};
const QByteArray APIKEY{"redacted (use your own here)"};
const QByteArray TIME_301{"5e20d8e0ba05e2483904a186"};
const QByteArray TIME_308{"5e20d9125f9e5b0d8832b556"};

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

	ClockifyUser me{"60ac148ef917c56d38c7017a", &manager};

	auto x = me.getRunningTimeEntry();

	if (parser.isSet(on) && !me.hasRunningTimeEntry())
		me.startTimeEntry(TIME_308); // 308
	else if (parser.isSet(off) && me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
	else if (parser.isSet(breakTime))
	{
		if (me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
		me.startTimeEntry(TIME_301);
	}
	else if (parser.isSet(endBreak))
	{
		if (me.hasRunningTimeEntry())
			me.stopCurrentTimeEntry();
		me.startTimeEntry(TIME_308);
	}
	else if (parser.isSet(running))
		return !me.hasRunningTimeEntry();
	else if (parser.isSet(working))
	{
		if (!me.hasRunningTimeEntry())
			return 1;
		else
			return me.getRunningTimeEntry()[0]["projectId"].get<QString>() == TIME_301; // get first (and only) json entry
	}

	return 0;
}
