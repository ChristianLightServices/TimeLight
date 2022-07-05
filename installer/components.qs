function Component()
{
    component.loaded.connect(this, Component.prototype.setUpInstallerPages);

    installer.valueChanged.connect((key, value) => { if (key === "TargetDir") Component.prototype.checkForExistingInstall(); });
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
            widget.globalInstall.checked = true;
        }
        var page = gui.pageByObjectName("DynamicInstallType");
        if (page != null)
        {
            page.entered.connect(this, Component.prototype.checkForExistingInstall);
        }
    }
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
    if (systemInfo.productType === "windows")
        installer.setValue("TargetDir", installer.environmentVariable("appdata") + "/Christian Light");
    else
        installer.setValue("TargetDir", "@ApplicationsDirUser@/Christian Light");
}

Component.prototype.globalInstallSelected = function()
{
    installer.setValue("TargetDir", "@ApplicationsDir@/Christian Light");
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType === "windows")
    {
        let appPath = installer.toNativeSeparators(installer.value("TargetDir") + "\\bin\\TimeLight.exe");
        component.addOperation("CreateShortcut", appPath, "@StartMenuDir@\\TimeLight.lnk");
        component.addOperation("CreateShortcut", appPath, "@DesktopDir@\\TimeLight.lnk");
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
    }
}
