/*
 * smart_attributes.c - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "smart_attributes.h"
#include <stddef.h>

/* Known SMART attributes with metadata */
static const smart_attribute_def_t attribute_definitions[] = {
    {0x01, "Read_Error_Rate", "Rate of hardware read errors", 0},
    {0x02, "Throughput_Performance", "Overall throughput performance", 0},
    {0x03, "Spin_Up_Time", "Time to spin up from stopped", 0},
    {0x04, "Start_Stop_Count", "Number of spindle start/stop cycles", 0},
    {0x05, "Reallocated_Sector_Ct", "Count of reallocated sectors", 1},
    {0x07, "Seek_Error_Rate", "Rate of seek errors", 0},
    {0x08, "Seek_Time_Performance", "Average seek performance", 0},
    {0x09, "Power_On_Hours", "Count of hours in power-on state", 0},
    {0x0A, "Spin_Retry_Count", "Number of retry attempts to spin up", 1},
    {0x0B, "Recalibration_Retries", "Number of recalibration retries", 0},
    {0x0C, "Power_Cycle_Count", "Number of power-on/off cycles", 0},
    {0x0D, "Soft_Read_Error_Rate", "Rate of software read errors", 0},
    {0xAA, "Available_Reserved_Space", "Available reserved space", 0},
    {0xAB, "SSD_Program_Fail_Count", "SSD program fail count", 1},
    {0xAC, "SSD_Erase_Fail_Count", "SSD erase fail count", 1},
    {0xAD, "SSD_Wear_Leveling_Count", "SSD wear leveling count", 0},
    {0xAE, "Unexpected_Power_Loss", "Count of unexpected power loss events", 0},
    {0xB7, "SATA_Downshift_Count", "SATA speed downshift count", 0},
    {0xB8, "End_to_End_Error", "End-to-end data path error", 1},
    {0xBB, "Reported_Uncorrect", "Reported uncorrectable errors", 1},
    {0xBC, "Command_Timeout", "Command timeout count", 0},
    {0xBD, "High_Fly_Writes", "High fly writes (head flying too high)", 1},
    {0xBE, "Airflow_Temperature", "Airflow temperature", 0},
    {0xBF, "G-Sense_Error_Rate", "G-sense error rate (shock)", 0},
    {0xC0, "Power-Off_Retract_Count", "Emergency head retract count", 0},
    {0xC1, "Load_Cycle_Count", "Head load/unload cycle count", 0},
    {0xC2, "Temperature_Celsius", "Current drive temperature", 0},
    {0xC3, "Hardware_ECC_Recovered", "ECC on-the-fly error count", 0},
    {0xC4, "Reallocation_Event_Count", "Remap event count", 1},
    {0xC5, "Current_Pending_Sector", "Unstable sectors pending remap", 1},
    {0xC6, "Offline_Uncorrectable", "Uncorrectable sector count", 1},
    {0xC7, "UltraDMA_CRC_Error_Count", "UDMA CRC error count", 0},
    {0xC8, "Write_Error_Rate", "Rate of write errors", 0},
    {0xC9, "Soft_Read_Error_Rate", "Off-track soft read error rate", 0},
    {0xCA, "Data_Address_Mark_Error", "Data address mark errors", 0},
    {0xCB, "Run_Out_Cancel", "ECC errors corrected by firmware", 0},
    {0xCC, "Soft_ECC_Correction", "Soft ECC correction count", 0},
    {0xCD, "Thermal_Asperity_Rate", "Thermal asperity rate", 0},
    {0xCE, "Flying_Height", "Head flying height", 0},
    {0xCF, "Spin_High_Current", "Spin-up current", 0},
    {0xD0, "Spin_Buzz", "Spin buzz count", 0},
    {0xD1, "Offline_Seek_Performance", "Seek performance during offline ops", 0},
    {0xDC, "Disk_Shift", "Disk shift relative to spindle", 0},
    {0xDD, "G-Sense_Error_Rate_2", "G-sense error rate (alternate)", 0},
    {0xDE, "Loaded_Hours", "Time spent with heads loaded", 0},
    {0xDF, "Load_Retry_Count", "Load/unload retry count", 0},
    {0xE0, "Load_Friction", "Head load friction", 0},
    {0xE1, "Load_Cycle_Count_2", "Load/unload cycle count (alternate)", 0},
    {0xE2, "Load_In_Time", "Time from start to fully loaded", 0},
    {0xE3, "Torque_Amplification", "Torque amplification factor", 0},
    {0xE4, "Power-Off_Retract_Cycle", "Power-off retract cycle count", 0},
    {0xE6, "GMR_Head_Amplitude", "GMR head amplitude", 0},
    {0xE7, "Temperature_Celsius_2", "Drive temperature (alternate)", 0},
    {0xE8, "Endurance_Remaining", "SSD endurance remaining", 0},
    {0xE9, "Power_On_Hours_2", "Power-on hours (alternate)", 0},
    {0xEA, "Average_Erase_Count", "SSD average erase count", 0},
    {0xEB, "Good_Block_Count", "SSD good block count", 0},
    {0xF0, "Head_Flying_Hours", "Time head spent flying", 0},
    {0xF1, "Total_LBAs_Written", "Total LBAs written", 0},
    {0xF2, "Total_LBAs_Read", "Total LBAs read", 0},
    {0xFA, "Read_Error_Retry_Rate", "Read error retry rate", 0},
    {0xFE, "Free_Fall_Protection", "Free fall protection events", 0},
};

const smart_attribute_def_t* get_attribute_definition(uint8_t id) {
    int num_defs = sizeof(attribute_definitions) / sizeof(attribute_definitions[0]);

    for (int i = 0; i < num_defs; i++) {
        if (attribute_definitions[i].id == id) {
            return &attribute_definitions[i];
        }
    }

    return NULL;  // Unknown attribute
}

int is_critical_attribute(uint8_t id) {
    const smart_attribute_def_t* def = get_attribute_definition(id);

    if (def != NULL) {
        return def->is_critical;
    }

    return 0;  // Unknown attributes are not critical by default
}
