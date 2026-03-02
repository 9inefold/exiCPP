//===- util/Log.java ------------------------------------------------===//
//
// Copyright (C) 2026 Ninefold
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//
///
/// Implements a simple logger.
///
//===----------------------------------------------------------------===//

package org.exicpp.util;

import org.exicpp.util.EvilDocumentHijacker;
import org.exicpp.util.LogLevel;

/** Simple logging interface */
public final class Log {
  /** If logging should be enabled at all */
  public static final boolean ENABLE_DEBUG_LOGGING = true;
  /** The default log level (ERROR) */
  public static final int LOG_DEFAULT = LogLevel.ERROR;
  /** The system provided log level. */
  private static final int SYSTEM_LOG;

  static { SYSTEM_LOG = getLogLevelFromSystem(); }

  /// Methods

  public static void error(String msg) {
    if (SYSTEM_LOG >= LogLevel.ERROR)
      System.err.print(msg);
  }

  public static void warn(String msg) {
    if (SYSTEM_LOG >= LogLevel.WARNING)
      System.err.print(msg);
  }

  public static void info(String msg) {
    if (ENABLE_DEBUG_LOGGING && SYSTEM_LOG >= LogLevel.INFO)
      System.err.print(msg);
  }

  public static void extra(String msg) {
    if (ENABLE_DEBUG_LOGGING && SYSTEM_LOG >= LogLevel.VERBOSE)
      System.err.print(msg);
  }

  public static void errorln(String msg) {
    if (SYSTEM_LOG >= LogLevel.ERROR)
      System.err.println(msg);
  }

  public static void warnln(String msg) {
    if (SYSTEM_LOG >= LogLevel.WARNING)
      System.err.println(msg);
  }

  public static void infoln(String msg) {
    if (ENABLE_DEBUG_LOGGING && SYSTEM_LOG >= LogLevel.INFO)
      System.err.println(msg);
  }

  public static void extraln(String msg) {
    if (ENABLE_DEBUG_LOGGING && SYSTEM_LOG >= LogLevel.VERBOSE)
      System.err.println(msg);
  }

  public static boolean hasError() {
    return SYSTEM_LOG >= LogLevel.ERROR;
  }

  public static boolean hasWarn() {
    return SYSTEM_LOG >= LogLevel.WARNING;
  }

  public static boolean hasInfo() {
    return ENABLE_DEBUG_LOGGING && SYSTEM_LOG >= LogLevel.INFO;
  }

  public static boolean hasExtra() {
    return ENABLE_DEBUG_LOGGING && SYSTEM_LOG >= LogLevel.VERBOSE;
  }

  /// Setup

  /** Gets LogLevel from system property "exicpp.loglevel" */
  private static int getLogLevelFromSystem() {
    String prop = EvilDocumentHijacker.getSystemProperty("exicpp.loglevel");
    return parseLogLevel(prop);
  }

  /** Gets LogLevel value from a string. */
  public static int parseLogLevel(String prop) {
    if (prop == null || prop.length() == 0)
      return LOG_DEFAULT;
    else if ("QUIET".equalsIgnoreCase(prop)  ||
             "SILENT".equalsIgnoreCase(prop) ||
             "OFF".equalsIgnoreCase(prop)    ||
             "NOTHING".equalsIgnoreCase(prop))
      return LogLevel.SILENT;
    else if ("ERROR".equalsIgnoreCase(prop))
      return LogLevel.ERROR;
    else if ("WARN".equalsIgnoreCase(prop) ||
             "WARNING".equalsIgnoreCase(prop))
      return LogLevel.WARN;
    else if ("INFO".equalsIgnoreCase(prop) ||
             "NOTE".equalsIgnoreCase(prop))
      return ENABLE_DEBUG_LOGGING ? LogLevel.INFO : LogLevel.WARN;
    else if ("EXTRA".equalsIgnoreCase(prop) ||
             "VERBOSE".equalsIgnoreCase(prop))
      return ENABLE_DEBUG_LOGGING ? LogLevel.VERBOSE : LogLevel.WARN;
    else
      return LOG_DEFAULT;
  }
}
