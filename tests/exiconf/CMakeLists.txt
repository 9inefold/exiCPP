include_guard(DIRECTORY)

configure_file(
  exiconf/constants.py.in
  ${CMAKE_CURRENT_SOURCE_DIR}/exiconf/constants.py
  @ONLY
)
