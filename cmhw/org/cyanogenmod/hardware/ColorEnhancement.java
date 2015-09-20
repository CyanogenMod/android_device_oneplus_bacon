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
 * Color enhancement panel control
 */
public class ColorEnhancement {

    private static String FILE_CE = "/sys/class/graphics/fb0/color_enhance";

    /**
     * Whether device supports CE
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() {
        File f = new File(FILE_CE);

        if(f.exists()) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * This method return the current activation status of CE
     *
     * @return boolean Must be false when CE is not supported or not activated, or
     * the operation failed while reading the status; true in any other case.
     */
    public static boolean isEnabled() {
	if (FileUtils.readOneLine(FILE_CE) != null) {
            if (Integer.parseInt(FileUtils.readOneLine(FILE_CE)) == 1) {
                return true;
            } else {
                return false;
            }
        }
	return false;
    }

    /**
     * This method allows to setup CE
     *
     * @param status The new CE status
     * @return boolean Must be false if CE is not supported or the operation
     * failed; true in any other case.
     */
    public static boolean setEnabled(boolean status) {
        if (status == true) {
            return FileUtils.writeLine(FILE_CE, "1");
        } else {
            return FileUtils.writeLine(FILE_CE, "0");
        }
    }
}
