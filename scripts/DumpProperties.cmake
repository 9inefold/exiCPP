include_guard(GLOBAL)

if(NOT DEFINED ENV{CMAKE_PROPERTY_LIST})
  execute_process(
    COMMAND cmake --help-property-list
    OUTPUT_VARIABLE CMAKE_PROPERTY_LIST
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  STRING(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
  STRING(REGEX REPLACE "\r?\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")

  # All supported languages with a target group.
  set(CMAKE_LANG_PFX "C;CXX;CUDA;OBJC;OBJCXX;Fortran;HIP;Swift")
  get_property(CMAKE_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
  list(REMOVE_ITEM CMAKE_languages "RC")
  
  list(JOIN CMAKE_languages "|" CMAKE_LANG_REGEX)
    list(FILTER CMAKE_LANG_PFX EXCLUDE REGEX "^(${CMAKE_LANG_REGEX})$")
  unset(CMAKE_LANG_REGEX)

  set(CMAKE_PFX_REMOVALS "AUTOGEN;AUTOMOC;AUTORCC;AUTOUIC;UNITY;${CMAKE_LANG_PFX}")
  if(NOT (CMAKE_SYSTEM_NAME STREQUAL "AIX"))
    list(APPEND CMAKE_PFX_REMOVALS "AIX")
  endif()
  if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Android"))
    list(APPEND CMAKE_PFX_REMOVALS "ANDROID")
  endif()
  if(NOT APPLE)
    list(APPEND CMAKE_PFX_REMOVALS "BUNDLE" "MACHO" "MACOS" "XCODE")
  endif()
  if(NOT ("CSharp" IN_LIST CMAKE_languages))
    list(APPEND CMAKE_PFX_REMOVALS "DOTNET")
  endif()
  if(NOT (CMAKE_GENERATOR MATCHES "Visual Studio"))
    list(APPEND CMAKE_PFX_REMOVALS "VS")
  endif()


  list(JOIN CMAKE_PFX_REMOVALS "|" CMAKE_REMOVALS_REGEX)
  list(FILTER CMAKE_PROPERTY_LIST EXCLUDE
    REGEX "^(LOCATION|${CMAKE_REMOVALS_REGEX})(_.+)?")
  #list(FILTER CMAKE_PROPERTY_LIST EXCLUDE REGEX "^.+_<CONFIG>$")
  list(TRANSFORM CMAKE_PROPERTY_LIST REPLACE "<CONFIG>" "${CMAKE_BUILD_TYPE}")

  set(CMAKE_PROPERTY_LIST_LANG "${CMAKE_PROPERTY_LIST}")
  list(FILTER CMAKE_PROPERTY_LIST EXCLUDE REGEX "^<LANG>.*")
  list(FILTER CMAKE_PROPERTY_LIST_LANG INCLUDE REGEX "^<LANG>.*")
  if("C" IN_LIST CMAKE_languages)
    set(CMAKE_languages_T "${CMAKE_PROPERTY_LIST_LANG}")
    list(TRANSFORM CMAKE_languages_T REPLACE "<LANG>" "C")
    list(PREPEND CMAKE_PROPERTY_LIST ${CMAKE_languages_T})
  endif()
  if("CXX" IN_LIST CMAKE_languages)
    set(CMAKE_languages_T "${CMAKE_PROPERTY_LIST_LANG}")
    list(TRANSFORM CMAKE_languages_T REPLACE "<LANG>" "CXX")
    list(PREPEND CMAKE_PROPERTY_LIST ${CMAKE_languages_T})
  endif()
  unset(CMAKE_languages_T)
  unset(CMAKE_PROPERTY_LIST_LANG)

  list(REMOVE_DUPLICATES CMAKE_PROPERTY_LIST)
  #list(REMOVE_ITEM CMAKE_PROPERTY_LIST "LOCATION")
  list(SORT CMAKE_PROPERTY_LIST COMPARE STRING)

  #list(JOIN CMAKE_PROPERTY_LIST "\n" property_list)
  #  message(STATUS "${property_list}")
  #unset(property_list)
  
  set(ENV{CMAKE_PROPERTY_LIST} "${CMAKE_PROPERTY_LIST}")
  unset(CMAKE_languages)
  unset(CMAKE_PFX_REMOVALS)
  unset(CMAKE_PROPERTY_LIST)
  unset(CMAKE_LANG_PFX)
endif()

# Lists all the properties of a given target.
# ALIAS targets will get resolved to the target they alias.
#  dump_target_properties(<target>)
function(dump_target_properties tgt)
  if(NOT TARGET ${tgt})
    message(STATUS "No target named `${tgt}`.")
    return()
  endif()

  get_target_property(tgt_alias ${tgt} ALIASED_TARGET)
  if("${tgt_alias}" STREQUAL "tgt_alias-NOTFOUND")
    set(tgt_name ${tgt})
  else()
    set(tgt_name ${tgt_alias})
  endif()

  set(PROP_LIST "")
  foreach(prop $ENV{CMAKE_PROPERTY_LIST})
    get_target_property(propval ${tgt_name} ${prop})
    if (propval)
      set(PROP_LIST "${PROP_LIST}\n[${tgt}] ${prop}: ${propval}")
    endif()
  endforeach()
  if(NOT ("" STREQUAL PROP_LIST))
    message(STATUS "${PROP_LIST}")
  else()
    message(STATUS "No properties found for target `${tgt}`.")
  endif()
endfunction()
