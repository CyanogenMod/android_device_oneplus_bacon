/*
 * Copyright (C) 2014 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.cyanogenmod.hardware;

import org.cyanogenmod.hardware.util.FileUtils;

import android.os.SystemProperties;

import java.io.File;

/**
 * Facemelt mode!
 *
 * Depends on CABC to be enabled on this hardware.
 */
public class SunlightEnhancement {

    private static String FILE_SRE = "/sys/class/graphics/fb0/sre";

    /**
     * Whether device supports SRE
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() {
        File f = new File(FILE_SRE);

        if(f.exists()) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * This method return the current activation status of SRE
     *
     * @return boolean Must be false when SRE is not supported or not activated, or
     * the operation failed while reading the status; true in any other case.
     */
    public static boolean isEnabled() {
        if (FileUtils.readOneLine(FILE_SRE) != null) {
            if (Integer.parseInt(FileUtils.readOneLine(FILE_SRE)) == 1) {
                return true;
            } else {
                return false;
            }
        }
       return false;

    }
    

    /**
     * This method allows to setup SRE
     *
     * @param status The new SRE status
     * @return boolean Must be false if SRE is not supported or the operation
     * failed; true in any other case.
     */
    public static boolean setEnabled(boolean status) {
        if (status == true) {
            return FileUtils.writeLine(FILE_SRE, "1");
        } else {
            return FileUtils.writeLine(FILE_SRE, "0");
        }
    }

    /**
     * Whether adaptive backlight (CABL / CABC) is required to be enabled
     *
     * @return boolean False if adaptive backlight is not a dependency
     */
    public static boolean isAdaptiveBacklightRequired() {
        return true;
    }
}
