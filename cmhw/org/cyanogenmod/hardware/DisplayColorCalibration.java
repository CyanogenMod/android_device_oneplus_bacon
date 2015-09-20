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

import java.io.File;
import java.util.Scanner;
import org.cyanogenmod.hardware.util.FileUtils;

public class DisplayColorCalibration {
    private static final String COLOR_FILE = "/sys/class/graphics/fb0/rgb";

    public static boolean isSupported() {
        File f = new File(COLOR_FILE);
        return f.exists();
    }

    public static int getMaxValue()  {
        return 32768;
    }

    public static int getMinValue()  {
        return 255;
    }

    public static int getDefValue() {
        return getMaxValue();
    }

    public static String getCurColors()  {
        return FileUtils.readOneLine(COLOR_FILE);
    }

    public static boolean setColors(String colors) {
        return FileUtils.writeLine(COLOR_FILE, colors);
    }
}
