//-----------------------------------------------------------------------------
// Copyright (C) 2012 Chalk <chalk.secu at gmail.com>
//               2015 Dake <thomas.cayrou at gmail.com>

// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Low frequency PCF7931 commands
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "proxmark3.h"
#include "ui.h"
#include "util.h"
#include "graph.h"
#include "cmdparser.h"
#include "cmddata.h"
#include "cmdmain.h"
#include "cmdlf.h"
#include "cmdlfpcf7931.h"

static int CmdHelp(const char *Cmd);

#define PCF7931_DEFAULT_INITDELAY 17500
#define PCF7931_DEFAULT_OFFSET_WIDTH 0
#define PCF7931_DEFAULT_OFFSET_POSITION 0

// Default values - Configuration
struct pcf7931_config configPcf = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    PCF7931_DEFAULT_INITDELAY,
    PCF7931_DEFAULT_OFFSET_WIDTH,
    PCF7931_DEFAULT_OFFSET_POSITION
};

// Resets the configuration settings to default values.
int pcf7931_resetConfig() {
    memset(configPcf.Pwd, 0xFF, sizeof(configPcf.Pwd));
    configPcf.InitDelay = PCF7931_DEFAULT_INITDELAY;
    configPcf.OffsetWidth = PCF7931_DEFAULT_OFFSET_WIDTH;
    configPcf.OffsetPosition = PCF7931_DEFAULT_OFFSET_POSITION;
    return 0;
}

int pcf7931_printConfig() {
    PrintAndLogEx(NORMAL, "Password (LSB first on bytes) : %s", sprint_hex(configPcf.Pwd, sizeof(configPcf.Pwd)));
    PrintAndLogEx(NORMAL, "Tag initialization delay      : %d us", configPcf.InitDelay);
    PrintAndLogEx(NORMAL, "Offset low pulses width       : %d us", configPcf.OffsetWidth);
    PrintAndLogEx(NORMAL, "Offset low pulses position    : %d us", configPcf.OffsetPosition);
    return 0;
}

static int usage_pcf7931_read() {
    PrintAndLogEx(NORMAL, "Usage: lf pcf7931 read [h] ");
    PrintAndLogEx(NORMAL, "This command tries to read a PCF7931 tag.");
    PrintAndLogEx(NORMAL, "Options:");
    PrintAndLogEx(NORMAL, "       h   This help");
    PrintAndLogEx(NORMAL, "Examples:");
    PrintAndLogEx(NORMAL, "      lf pcf7931 read");
    return 0;
}

static int usage_pcf7931_write() {
    PrintAndLogEx(NORMAL, "Usage: lf pcf7931 write [h] <block address> <byte address> <data>");
    PrintAndLogEx(NORMAL, "This command tries to write a PCF7931 tag.");
    PrintAndLogEx(NORMAL, "Options:");
    PrintAndLogEx(NORMAL, "       h              This help");
    PrintAndLogEx(NORMAL, "       blockaddress   Block to save [0-7]");
    PrintAndLogEx(NORMAL, "       byteaddress    Index of byte inside block to write [0-15]");
    PrintAndLogEx(NORMAL, "       data           one byte of data (hex)");
    PrintAndLogEx(NORMAL, "Examples:");
    PrintAndLogEx(NORMAL, "      lf pcf7931 write 2 1 FF");
    return 0;
}

static int usage_pcf7931_config() {
    PrintAndLogEx(NORMAL, "Usage: lf pcf7931 config [h] [r] <pwd> <delay> <offset width> <offset position>");
    PrintAndLogEx(NORMAL, "This command tries to set the configuration used with PCF7931 commands");
    PrintAndLogEx(NORMAL, "The time offsets could be useful to correct slew rate generated by the antenna");
    PrintAndLogEx(NORMAL, "Caling without some parameter will print the current configuration.");
    PrintAndLogEx(NORMAL, "Options:");
    PrintAndLogEx(NORMAL, "       h       This help");
    PrintAndLogEx(NORMAL, "       r       Reset configuration to default values");
    PrintAndLogEx(NORMAL, "       pwd     Password, hex, 7bytes, LSB-order");
    PrintAndLogEx(NORMAL, "       delay   Tag initialization delay (in us) decimal");
    PrintAndLogEx(NORMAL, "       offset  Low pulses width (in us) decimal");
    PrintAndLogEx(NORMAL, "       offset  Low pulses position (in us) decimal");
    PrintAndLogEx(NORMAL, "Examples:");
    PrintAndLogEx(NORMAL, "      lf pcf7931 config");
    PrintAndLogEx(NORMAL, "      lf pcf7931 config r");
    PrintAndLogEx(NORMAL, "      lf pcf7931 config 11223344556677 20000");
    PrintAndLogEx(NORMAL, "      lf pcf7931 config 11223344556677 17500 -10 30");
    return 0;
}

static int CmdLFPCF7931Read(const char *Cmd) {

    uint8_t ctmp = param_getchar(Cmd, 0);
    if (ctmp == 'H' || ctmp == 'h') return usage_pcf7931_read();

    PacketResponseNG resp;
    clearCommandBuffer();
    SendCommandOLD(CMD_PCF7931_READ, 0, 0, 0, NULL, 0);
    if (!WaitForResponseTimeout(CMD_ACK, &resp, 2500)) {
        PrintAndLogEx(WARNING, "command execution time out");
        return 1;
    }
    return 0;
}

static int CmdLFPCF7931Config(const char *Cmd) {

    uint8_t ctmp = param_getchar(Cmd, 0);
    if (ctmp == 0) return pcf7931_printConfig();
    if (ctmp == 'H' || ctmp == 'h') return usage_pcf7931_config();
    if (ctmp == 'R' || ctmp == 'r') return pcf7931_resetConfig();

    if (param_gethex(Cmd, 0, configPcf.Pwd, 14)) return usage_pcf7931_config();

    configPcf.InitDelay = (param_get32ex(Cmd, 1, 0, 10) & 0xFFFF);
    configPcf.OffsetWidth = (int)(param_get32ex(Cmd, 2, 0, 10) & 0xFFFF);
    configPcf.OffsetPosition = (int)(param_get32ex(Cmd, 3, 0, 10) & 0xFFFF);

    pcf7931_printConfig();
    return 0;
}

static int CmdLFPCF7931Write(const char *Cmd) {

    uint8_t ctmp = param_getchar(Cmd, 0);
    if (strlen(Cmd) < 1 || ctmp == 'h' || ctmp == 'H') return usage_pcf7931_write();

    uint8_t block = 0, bytepos = 0, data = 0;

    if (param_getdec(Cmd, 0, &block)) return usage_pcf7931_write();
    if (param_getdec(Cmd, 1, &bytepos)) return usage_pcf7931_write();

    if ((block > 7) || (bytepos > 15)) return usage_pcf7931_write();

    data = param_get8ex(Cmd, 2, 0, 16);

    PrintAndLogEx(NORMAL, "Writing block: %d", block);
    PrintAndLogEx(NORMAL, "          pos: %d", bytepos);
    PrintAndLogEx(NORMAL, "         data: 0x%02X", data);

    uint32_t buf[10]; // TODO sparse struct, 7 *bytes* then words at offset 4*7!
    memcpy(buf, configPcf.Pwd, sizeof(configPcf.Pwd));
    buf[7] = (configPcf.OffsetWidth + 128);
    buf[8] = (configPcf.OffsetPosition + 128);
    buf[9] = configPcf.InitDelay;

    clearCommandBuffer();
    SendCommandOLD(CMD_PCF7931_WRITE, block, bytepos, data, buf, sizeof(buf));
    //no ack?
    return 0;
}

static command_t CommandTable[] = {
    {"help",   CmdHelp,            AlwaysAvailable, "This help"},
    {"read",   CmdLFPCF7931Read,   IfPm3Present,    "Read content of a PCF7931 transponder"},
    {"write",  CmdLFPCF7931Write,  IfPm3Present,    "Write data on a PCF7931 transponder."},
    {"config", CmdLFPCF7931Config, AlwaysAvailable, "Configure the password, the tags initialization delay and time offsets (optional)"},
    {NULL, NULL, NULL, NULL}
};

static int CmdHelp(const char *Cmd) {
    (void)Cmd; // Cmd is not used so far
    CmdsHelp(CommandTable);
    return 0;
}

int CmdLFPCF7931(const char *Cmd) {
    clearCommandBuffer();
    return CmdsParse(CommandTable, Cmd);
}

