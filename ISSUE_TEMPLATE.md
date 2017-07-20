### Before you submit your issue, please make sure that

- [ ] you use the latest version of your branch (master/development/...; dont forget to git pull and cmake + make again)
- [ ] your issue is reproducable OR you know exactly what was wrong

If you have just a questions or want to talk about the application, please consider posting on https://67.ip-145-239-77.eu.
You can log in with your **github.com** account.

### When you submit your issue, please make sure to add the following informations

- [ ] what operation system are you using?
- [ ] what version of the application are you using?
- [ ] what branch of the application are you using?
- [ ] what executable of the application are you using? (from releases, self compiled, dockerfile, ...)
- [ ] if you compiled the application by yourself, what cmake settings did you use?

Please tell exactly what was wrong, how often it is or was wrong, how it normally works and what you changed.
Also it would be really helpful to get a log file and your configuration file that you can attach by simply drag and drop it into the issue window.

Thank you very much!

You can use the following template and fill in your data (simply delete the rest of the issue template):

**OS**: Linux Mint 18.2
**APP-VERSION**: 1.6.0
**APP-BRANCH**: development
**EXECUTABLE**: self compiled
**CMAKE-SETTINGS**: cmake -DCMAKE_BUILD_TYPE=RELEASE -DCPU_INSTRUCTION_SET=AVX
