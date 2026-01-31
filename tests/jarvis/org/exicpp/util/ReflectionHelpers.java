//===- util/EvilDocumentHijacker.java -------------------------------===//
//===- util/ReflectionHelpers.java ----------------------------------===//
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
/// \file
/// A VERY evil utility class which modifies xerces parser internals.
///
//===----------------------------------------------------------------===//

package org.exicpp.util;

import java.lang.System;
import java.lang.NoSuchFieldException;
import java.lang.reflect.*;

public class ReflectionHelpers {

  ////////////////////////////////////////////////////////////////////////
  // Reflection Methods

  public static Field locateMemberInSuperclasses(Class<?> clazz, String name)
      throws NoSuchFieldException, SecurityException {
    final Class<?> objectClazz = java.lang.Object.class;
    Class<?> storedclazz = clazz;
    while (clazz != objectClazz && clazz != null) {
      try {
        return clazz.getDeclaredField(name);
      } catch (NoSuchFieldException e) {
        clazz = clazz.getSuperclass();
      }
    }
    throw new NoSuchFieldException("Could not find " + name
                                 + " in any superclasses of "
                                 + storedclazz.getName());
  }

  public static Object getObjectField(Object obj, Field field) {
    if (obj == null || !field.canAccess(obj))
      return null;
    try {
      return field.get(obj);
    } catch (IllegalArgumentException e) {
      return null;
    } catch (IllegalAccessException e) {
      System.err.println(e.getMessage());
      return null;
    }
  }

  public static <T> void setObjectField(Object obj, T val, Field field) {
    if (obj == null || !field.canAccess(obj))
      return;
    try {
      field.set(obj, val);
    } catch (IllegalArgumentException e) {
    } catch (IllegalAccessException e) {
      System.err.println(e.getMessage());
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // Printing Info

  public static void printClassMethods(Class<?> clazz) {
    System.out.format("%s {", clazz.getTypeName());
    Method[] methods = clazz.getDeclaredMethods();
    if (methods.length != 0)
      System.out.println("");
    
    StringBuilder sb = new StringBuilder();
    sb.append("  ");

    for (Method method : methods) {
      sb.delete(2, sb.length());
      // Print modifiers
      int mods = method.getModifiers();
      if (Modifier.isStatic(mods))
        sb.append("static ");
      // Print signature
      sb.append(method.getName());
      sb.append('(');
      Parameter[] params = method.getParameters();
      if (params.length > 0) {
        sb.append(params[0].getType().getName());
        for (int i = 1; i < params.length; ++i) {
          sb.append(", ");
          sb.append(params[i].getType().getName());
        }
      }
      sb.append(") -> ");
      sb.append(method.getReturnType().getName());
      System.out.println(sb.toString());
    }
    System.out.println("}");
  }

  public static void printSuperclassMethods(Class<?> clazz) {
    final Class<?> objectClazz = java.lang.Object.class;
    while (clazz != objectClazz && clazz != null) {
      printClassMethods(clazz);
      clazz = clazz.getSuperclass();
    }
  }

  public static void printFields(Field[] fields, boolean printStatic) {
    for (Field field : fields) {
      if (!printStatic) {
        if (Modifier.isStatic(field.getModifiers()))
          continue;
      }
      String name = field.getName();
      String type = field.getType().getName();
      System.out.format("  %s: %s%n", name, type);
    }
  }

  public static void printClassFields(Class<?> clazz, boolean printStatic) {
    System.out.format("%s {", clazz.getTypeName());
    Field[] fields = clazz.getDeclaredFields();
    if (fields.length != 0)
      System.out.println("");
    printFields(fields, printStatic);
    System.out.println("}");
  }

  public static void printClassFields(Class<?> clazz) {
    printClassFields(clazz, false);
  }

  public static void printSuperclassFields(Class<?> clazz) {
    final Class<?> objectClazz = java.lang.Object.class;
    while (clazz != objectClazz && clazz != null) {
      printClassFields(clazz);
      clazz = clazz.getSuperclass();
    }
  }
}
