include_guard(DIRECTORY)
include(ConfigureFiles)

configure_inline(Config/Config.inc.in @ONLY)
configure_inline(Config/ABIBreak.inc.in @ONLY)
configure_inline(Config/XML.inc.in @ONLY)
