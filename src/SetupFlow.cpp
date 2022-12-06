#include "SetupFlow.h"

#include <QInputDialog>
#include <QMessageBox>

#include "ClockifyManager.h"
#include "Logger.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "TimeCampManager.h"

namespace logs = TimeLight::logs;

SetupFlow::SetupFlow(TrayIcons *parent)
    : QObject{parent},
      m_parent{parent}
{}

void SetupFlow::cancelFlow()
{
    m_stage = Stage::NotStarted;
}

SetupFlow::Result SetupFlow::runNextStage()
{
    switch (m_stage)
    {
    case Stage::NotStarted:
        m_stage = Stage::TimeService;
        return runTimeServiceStage();
    case Stage::TimeService:
        m_stage = Stage::ApiKey;
        return runApiKeyStage();
    case Stage::ApiKey:
        m_stage = Stage::Workspace;
        return runWorkspaceStage();
    case Stage::Workspace:
        m_stage = Stage::Project;
        return runProjectStage();
    case Stage::Project:
        m_stage = Stage::Done;
        return SetupFlow::Result::Valid;
    default:
        m_stage = Stage::NotStarted;
        return SetupFlow::Result::Valid;
    }
}

SetupFlow::Result SetupFlow::runPreviousStage()
{
    switch (m_stage)
    {
    case Stage::Done:
        m_stage = Stage::Project;
        return runProjectStage();
    case Stage::Project:
        m_stage = Stage::Workspace;
        return runWorkspaceStage();
    case Stage::Workspace:
        m_stage = Stage::ApiKey;
        return runApiKeyStage();
    case Stage::ApiKey:
        m_stage = Stage::TimeService;
        return runTimeServiceStage();
    default:
        m_stage = Stage::NotStarted;
        return SetupFlow::Result::Valid;
    }
}

SetupFlow::Result SetupFlow::rerunCurrentStage()
{
    switch (m_stage)
    {
    case Stage::TimeService:
        return runTimeServiceStage();
    case Stage::ApiKey:
        return runApiKeyStage();
    case Stage::Workspace:
        return runWorkspaceStage();
    case Stage::Project:
        return runProjectStage();
    default:
        return SetupFlow::Result::Valid;
    }
}

void SetupFlow::resetNextStage()
{
    switch (m_stage)
    {
    case Stage::Done:
        resetTimeServiceStage();
        break;
    case Stage::TimeService:
        resetApiKeyStage();
        break;
    case Stage::ApiKey:
        resetWorkspaceStage();
        break;
    case Stage::Workspace:
        resetProjectStage();
        break;
    default:
        break;
    }
}

void SetupFlow::resetPreviousStage()
{
    switch (m_stage)
    {
    case Stage::ApiKey:
        resetTimeServiceStage();
        break;
    case Stage::Workspace:
        resetApiKeyStage();
        break;
    case Stage::Project:
        resetWorkspaceStage();
        break;
    case Stage::Done:
        resetProjectStage();
        break;
    default:
        break;
    }
}

void SetupFlow::resetCurrentStage()
{
    switch (m_stage)
    {
    case Stage::TimeService:
        resetTimeServiceStage();
        break;
    case Stage::ApiKey:
        resetApiKeyStage();
        break;
    case Stage::Workspace:
        resetWorkspaceStage();
        break;
    case Stage::Project:
        resetProjectStage();
        break;
    default:
        break;
    }
}

SetupFlow::Result SetupFlow::runTimeServiceStage() const
{
    if (Settings::instance()->timeService().isEmpty())
    {
        logs::app()->trace("Initializing time service");
        bool ok{false};
        QString service = QInputDialog::getItem(nullptr,
                                                QObject::tr("Time service"),
                                                QObject::tr("Choose which time service to use:"),
                                                QStringList{} << QStringLiteral("Clockify") << QStringList("TimeCamp"),
                                                0,
                                                false,
                                                &ok);
        if (service == QStringLiteral("Clockify"))
            Settings::instance()->setTimeService(QStringLiteral("com.clockify"));
        else if (service == QStringLiteral("TimeCamp"))
        {
            Settings::instance()->setTimeService(QStringLiteral("com.timecamp"));
            Settings::instance()->setEventLoopInterval(15000);
        }

        return ok ? (service.isEmpty() ? SetupFlow::Result::Invalid : SetupFlow::Result::Valid) :
                    SetupFlow::Result::Canceled;
    }

    return SetupFlow::Result::Valid;
}

SetupFlow::Result SetupFlow::runApiKeyStage() const
{
    Result r = SetupFlow::Result::Valid;

    if (Settings::instance()->apiKey().isEmpty())
    {
        logs::app()->trace("Initializing API key");
        bool ok{false};
        QString newKey = QInputDialog::getText(
            nullptr, QObject::tr("API key"), QObject::tr("Enter your API key:"), QLineEdit::Normal, QString{}, &ok);
        if (ok)
        {
            Settings::instance()->setApiKey(newKey);
            if (m_parent->m_manager)
                m_parent->m_manager->setApiKey(newKey);
        }

        r = ok ? (newKey.isEmpty() ? SetupFlow::Result::Invalid : SetupFlow::Result::Valid) : SetupFlow::Result::Canceled;
    }

    if (r == SetupFlow::Result::Valid)
    {
        if (Settings::instance()->timeService() == QStringLiteral("com.clockify"))
            m_parent->initializeManager<ClockifyManager>();
        else if (Settings::instance()->timeService() == QStringLiteral("com.timecamp"))
            m_parent->initializeManager<TimeCampManager>();
        else
        {
            // since the service is invalid, we'll just select Clockify...
            Settings::instance()->setTimeService("com.clockify");
            m_parent->initializeManager<ClockifyManager>();
        }
    }

    return r;
}

SetupFlow::Result SetupFlow::runWorkspaceStage() const
{
    Result r = SetupFlow::Result::Valid;

    if (Settings::instance()->workspaceId().isEmpty())
    {
        logs::app()->trace("Initializing workspace");
        auto workspaces = m_parent->m_manager->workspaces();
        QStringList names;
        for (const auto &w : workspaces)
            names << w.name();
        bool ok{false};
        QString workspace = QInputDialog::getItem(nullptr,
                                                  QObject::tr("Workspace"),
                                                  QObject::tr("Choose which workspace to track time on:"),
                                                  names,
                                                  0,
                                                  false,
                                                  &ok);

        if (ok)
            for (const auto &w : workspaces)
                if (w.name() == workspace)
                {
                    Settings::instance()->setWorkspaceId(w.id());
                    break;
                }

        r = ok ? (Settings::instance()->workspaceId().isEmpty() ? SetupFlow::Result::Invalid : SetupFlow::Result::Valid) :
                 SetupFlow::Result::Canceled;
    }
    if (r == SetupFlow::Result::Valid)
        m_parent->m_manager->setWorkspaceId(Settings::instance()->workspaceId());

    return r;
}

SetupFlow::Result SetupFlow::runProjectStage() const
{
    auto validProjectSet = [] {
        return (Settings::instance()->useLastProject() || !Settings::instance()->projectId().isEmpty()) &&
               (Settings::instance()->useSeparateBreakTime() ? !Settings::instance()->breakTimeId().isEmpty() : true);
    };

    if (!validProjectSet())
    {
        logs::app()->trace("Initializing project ID(s)");
        SettingsDialog d{m_parent->m_manager,
                         {{QStringLiteral("Clockify"), QStringLiteral("com.clockify")},
                          {QStringLiteral("TimeCamp"), QStringLiteral("com.timecamp")}}};
        d.switchToPage(SettingsDialog::Pages::ProjectPage);
        QMessageBox::information(nullptr,
                                 QObject::tr("Select a project"),
                                 QObject::tr("Please select a default project in the following dialog."));
        d.exec();
    }

    return validProjectSet() ? SetupFlow::Result::Valid : SetupFlow::Result::Invalid;
}

void SetupFlow::resetTimeServiceStage() const
{
    Settings::instance()->setTimeService({});
}

void SetupFlow::resetApiKeyStage() const
{
    Settings::instance()->setApiKey({});
}

void SetupFlow::resetWorkspaceStage() const
{
    Settings::instance()->setWorkspaceId({});
}

void SetupFlow::resetProjectStage() const
{
    Settings::instance()->setProjectId({});
    Settings::instance()->setBreakTimeId({});
}
