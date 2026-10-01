#ifndef __SD_FUNCTIONS_H
#define __SD_FUNCTIONS_H
#include <globals.h>

#include "idf/idf_update.h"
#include <SPI.h>

#include <FFat.h>
#include <FS.h>
#include <SD.h>
#if !defined(SDM_SD)
#include <SD_MMC.h>
#endif
extern SPIClass sdcardSPI;

bool setupSdCard();

bool deleteFromSd(const String &path);

bool renameFile(const String &path, const String &filename);

bool copyFile(const String &path);

bool pasteFile(const String &path);

bool createFolder(String path);

void readFs(String &folder, std::vector<Option> &opt);

bool sortList(const Option &a, const Option &b);

String loopSD(bool filePicker = false);

// Controls the two interactive prompts inside updateFromSD(). The on-device UI
// leaves this defaulted, so its behaviour is unchanged. A headless caller (the
// serial console) must set interactive=false: loopOptions() only unblocks on
// physical touch/keyboard, and the serial console task is the sole reader of
// Serial, so a prompt raised from there deadlocks the console with no output.
//
// Note the flags cannot express the headless case on their own -- askSpiffs=false
// means "do not copy data" and autoBackup=false means "always restore" -- which is
// why copyData/restoreBackup are carried separately.
struct SdInstallOptions {
    bool interactive = true;    // may call loopOptions()
    bool copyData = true;       // non-interactive: copy the image's data partition
    bool restoreBackup = false; // non-interactive: fresh install, ignore old backup
};

void updateFromSD(const String &path, const SdInstallOptions &installOptions = SdInstallOptions());

bool performDATAUpdate(Stream &updateSource, size_t updateSize, const char *label);

#endif
