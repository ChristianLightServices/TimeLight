function Component()
{
    component.loaded.connect(this, Component.prototype.setUpInstallerPages);
}

Component.prototype.setUpInstallerPages = function()
{
    if (installer.addWizardPage(component, "InstallType", QInstaller.TargetDirectory))
    {
        installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
        var widget = gui.pageWidgetByObjectName("DynamicInstallType");
        if (widget != null)
        {
            widget.windowTitle = "Installation type";
            widget.globalInstall.toggled.connect(this, Component.prototype.globalInstallSelected);
            widget.localInstall.toggled.connect(this, Component.prototype.localInstallSelected);

            if (systemInfo.productType === "windows")
                installer.setValue("__timelight_installer_install_type", installer.value("HKCU\\SOFTWARE\\Christian Light\\TimeLight\\__timelight_installer_install_type"));
            let installType = installer.value("__timelight_installer_install_type", "global");
            if (installType == "global")
                widget.globalInstall.checked = true;
            else
                widget.localInstall.checked = true;
        }
        var page = gui.pageByObjectName("DynamicInstallType");
        if (page != null)
        {
            page.entered.connect(this, Component.prototype.checkForExistingInstall);
        }
    }

    installer.valueChanged.connect((key, value) => { if (key === "TargetDir") Component.prototype.checkForExistingInstall(); });
}

Component.prototype.checkForExistingInstall = function()
{
    let oldInstaller = installer.value("TargetDir") + "/maintenancetool.exe";
    if (installer.fileExists(oldInstaller))
        if (QMessageBox.question("uninstallprevious.question",
                                 "Remove previous installation",
                                 "Do you want to remove the previous installation of TimeLight before installing this version?",
                                 QMessageBox.Yes | QMessageBox.No)
                === QMessageBox.Yes)
            installer.execute(oldInstaller, ["PleaseDeleteMeWithNaryAWordOfProtest=AbsolutelyGoRightAhead"]);
}

Component.prototype.localInstallSelected = function()
{
    installer.setValue("__timelight_installer_install_type", "local");
    if (systemInfo.productType === "windows")
        installer.setValue("TargetDir", installer.environmentVariable("appdata") + "/Christian Light");
    else
        installer.setValue("TargetDir", "@ApplicationsDirUser@/Christian Light");
}

Component.prototype.globalInstallSelected = function()
{
    installer.setValue("TargetDir", "@ApplicationsDir@/Christian Light");
    installer.setValue("__timelight_installer_install_type", "global");
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType === "windows")
    {
        let appPath = installer.toNativeSeparators(installer.value("TargetDir") + "\\bin\\TimeLight.exe");
        component.addOperation("CreateShortcut", appPath, "@StartMenuDir@\\TimeLight.lnk");
        component.addOperation("CreateShortcut", appPath, "@DesktopDir@\\TimeLight.lnk");
        // run on startup
        component.addOperation("Execute",
                               installer.environmentVariable("systemroot") + "\\System32\\reg.exe",
                               "ADD",
                               "HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                               "/v",
                               "TimeLight",
                               "/t",
                               "REG_SZ",
                               "/d",
                               appPath,
                               "/f",
                               "UNDOEXECUTE",
                               installer.environmentVariable("systemroot") + "\\System32\\reg.exe",
                               "DELETE",
                               "HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                               "/v",
                               "TimeLight",
                               "/f");
        // stash install type
        component.addOperation("Execute",
                               installer.environmentVariable("systemroot") + "\\System32\\reg.exe",
                               "ADD",
                               "HKCU\\SOFTWARE\\Christian Light\\TimeLight",
                               "/v",
                               "__timelight_installer_install_type",
                               "/t",
                               "REG_SZ",
                               "/d",
                               installer.value("__timelight_installer_install_type", "global"),
                               "/f",
                               "UNDOEXECUTE",
                               installer.environmentVariable("systemroot") + "\\System32\\reg.exe",
                               "DELETE",
                               "HKCU\\SOFTWARE\\Christian Light\\TimeLight",
                               "/v",
                               "__timelight_installer_install_type",
                               "/f");
    }
}
