# Java

![Jarvis pick a name that doesn't make jpype have an aneurysm](./1238cd21391.png)

Since xerces doesn't expose a bunch of important things, and you can't change
the value of `final` fields in Java 12+, I have to crack open xerces and recreate
a bunch of features myself.

The following have been recreated:

- `org.apache.xerces.jaxp.SAXParserFactoryImpl`
- `org.apache.xerces.jaxp.SAXParserFactory`
- `org.apache.xerces.parsers.XIncludeAwareParserConfiguration`
- `org.openexi.sax.Transmogrifier`

*Jar har har!*
