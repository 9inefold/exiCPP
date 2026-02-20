//===- util/XConstants.java -----------------------------------------===//
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

public class XConstants {

  /// Features

  /** exicpp features prefix ("http://exicpp.org/xml/features/"). */
  public static final String EXICPP_FEATURE_PREFIX = "http://exicpp.org/xml/features/";

  /** Embed escape sequences */
  public static final String EMBED_ESCAPE_SEQUENCES = "scanner/embed-escape-sequences";

  /// Properties

  /** exicpp properties prefix ("http://exicpp.org/xml/properties/"). */
  public static final String EXICPP_PROPERTY_PREFIX = "http://exicpp.org/xml/properties/";

  /** Embed escape sequences */
  public static final String ESCAPE_HANDLER_PROPERTY = "escape-handler";

  /** Holds the handler of the list of features/properties */
  public static final String CONFIG_REFLECTOR_PROPERTY = "config-reflector";

  /// XML

  /** <!CDATA[ */
  public static final char[] CDATA_START = {'<','!','[','C','D','A','T','A','['};
  /** ]]> */
  public static final char[] CDATA_END = {']',']','>'};
}
