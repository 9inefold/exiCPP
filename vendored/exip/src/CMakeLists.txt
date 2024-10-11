if(EXIP_NEW_HASHTABLE AND EXIP_INLINE_HASHKEY)
  set(HASHTABLE_SRC "new_hashtable.c")
elseif(EXIP_NEW_HASHTABLE)
  set(HASHTABLE_SRC "new_hashtable_nalloc.c")
else()
  set(HASHTABLE_SRC "hashtable.c")
endif()

# Source files for the common module
include_items(COMMON_SRC "common/src"
  "ASCII_stringManipulate.c"
  "contentHandler.c"
  "dynamicArray.c"
  "errorHandle.c"
  ${HASHTABLE_SRC}
  "memManagement.c"
  "procTypes.c"
)

# Source files for the contentIO module
include_items(CONTENT_IO_SRC "contentIO/src"
  "bodyDecode.c"
  "bodyEncode.c"
  "EXIParser.c"
  "EXISerializer.c"
  "headerDecode.c"
  "headerEncode.c"
  "initSchemaInstance.c"
  "staticEXIOptions.c"
)

# Source files for the grammar module
include_items(GRAMMAR_SRC "grammar/src"
  "grammars.c"
)

# Source files for the grammar generation module
include_items(GRAMMAR_GEN_SRC "grammarGen/src"
  "genUtils.c"
  "grammarGenerator.c"
  "protoGrammars.c"
  "treeTableBuild.c"
  "treeTableManipulate.c"
  "treeTableToGrammars.c"
)

# Source files for the schema grammar generation module
include_items(GRAMMAR_GEN_SCHEMA_SRC "grammarGen/xmlSchema"
  "staticXmlSchema.c"
)

# Source files for the streamIO module
include_items(STREAM_IO_SRC "streamIO/src"
  "ioUtil.c"
  "streamDecode.c"
  "streamEncode.c"
  "streamRead.c"
  "streamWrite.c"
)

# Source files for the string tables module
include_items(STRING_TABLES_SRC "stringTables/src"
  "sTables.c"
)

set_parent(
  COMMON_SRC
  CONTENT_IO_SRC
  GRAMMAR_SRC
  GRAMMAR_GEN_SRC
  GRAMMAR_GEN_SCHEMA_SRC
  STREAM_IO_SRC
  STRING_TABLES_SRC
)
