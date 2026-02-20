//===- util/XStacktrace.java ----------------------------------------===//
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

package org.exicpp.util;

import java.io.PrintStream;
import java.lang.Exception;
import java.lang.StackTraceElement;
import java.lang.System;

public class XStacktrace {
  public static final int MAX_DEFAULT = 10000;

  public static void print(int skip, int max, PrintStream ps) {
    skip = (skip >= 0) ? skip + 1 : 0;
    max = max + skip;

    StackTraceElement[] trace = null;
    try { throw new Exception(); }
    catch (Exception e) {
      trace = e.getStackTrace();
    }

    if (trace == null || skip >= trace.length) {
      ps.println("> [no stacktrace]\n");
      return;
    }

    final int E = max < trace.length ? max : trace.length;
    for (int I = skip; I < E; ++I) {
      StackTraceElement t = trace[I];
      String extra = "";
      if (t.getFileName() != null)
        extra = t.getFileName();
      if (t.getLineNumber() >= 0) {
        if (extra.length() != 0)
          extra += ":";
        extra += t.getLineNumber();
      }
      if (extra.length() != 0)
        extra = " [" + extra + "]";
      ps.format(" > %s.%s%s\n",
        t.getClassName(), t.getMethodName(), extra);
    }
    ps.print("\n");
  }

  public static void print(int skip, PrintStream ps) {
    print(skip + 1, MAX_DEFAULT, System.out);
  }

  public static void print(PrintStream ps) {
    print(1, MAX_DEFAULT, ps);
  }

  public static void print(int skip, int max) {
    print(skip + 1, max, System.out);
  }

  public static void print(int skip) {
    print(skip + 1, MAX_DEFAULT, System.out);
  }

  public static void print() {
    print(1, MAX_DEFAULT, System.out);
  }
}
