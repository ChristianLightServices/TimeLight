function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType === "windows")
    {
        component.addOperation("CreateShortcut", "@TargetDir@/bin/TimeLight.exe", "@StartMenuDir@/TimeLight.lnk");
        component.addOperation("CreateShortcut", "@TargetDir@/bin/TimeLight.exe", "@DesktopDir@/TimeLight.lnk");
        // TODO: figure out how to do this (probably conditionally)
        // component.addOperation("CreateShortcut", "@TargetDir@/bin/TimeLight.exe", "@StartupDir@/TimeLight.lnk");
    }
}
