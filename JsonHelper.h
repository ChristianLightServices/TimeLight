#ifndef JSONHELPER_H
#define JSONHELPER_H

#include <QString>
#include <QByteArray>
#include <QDateTime>

#include <memory>

#include <nlohmann/json.hpp>

// easy conversion of json <=> QString
// (no, this is NOT a C++20 spaceship operator!)
namespace nlohmann
{
//	using QJson = basic_json<QMap, QVector, QString, bool, std::int64_t, std::uint64_t, double, std::allocator, adl_serializer, QVector<std::uint8_t>>;

	template<> struct adl_serializer<QString>
	{
		static void to_json(json &j, const QString &opt)
		{
			j = opt.toStdString();
		}

		static void from_json(const json &j, QString &opt)
		{
			opt = QString::fromStdString(j.get<std::string>());
		}
	};

	template<> struct adl_serializer<QByteArray>
	{
		static void to_json(json &j, const QByteArray &opt)
		{
			j = opt.toStdString();
		}

		static void from_json(const json &j, QByteArray &opt)
		{
			opt = QByteArray::fromStdString(j.get<std::string>());
		}

//		// these are required for QJson (QString stuff isn't because it is QJson's StringType)
//		static void to_json(QJson &j, const QByteArray &opt)
//		{
//			j = opt;
//		}

//		static void from_json(const QJson &j, QByteArray &opt)
//		{
//			opt = j.get<QString>();
//		}
	};

	template<> struct adl_serializer<QDateTime>
	{
		static void to_json(json &j, const QDateTime &opt)
		{
			j = opt.toString("yyyy-MM-ddThh:mm:ssZ");
		}

		static void from_json(const json &j, QDateTime &opt)
		{
			opt = QDateTime::fromString(j.get<QString>(), "yyyy-MM-ddThh:mm:ssZ");
		}
	};
}

#endif // JSONHELPER_H
