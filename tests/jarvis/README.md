# Java

![Jarvis pick a name that doesn't make jpype have an aneurysm](./1238cd21391.png)

Since xerces doesn't expose a bunch of important things, and you can't change
the value of `final` fields in Java 12+, I have to crack open xerces and recreate
a bunch of features myself.

The following have been recreated or extended:

- `org.apache.xerces.impl.XML(NS)DocumentScanner`
- `org.apache.xerces.impl.XMLEntityManager`
- `org.apache.xerces.jaxp.SAXParserFactoryImpl`
- `org.apache.xerces.jaxp.SAXParserFactory`
- `org.apache.xerces.parsers.XIncludeAwareParserConfiguration`
- `org.openexi.sax.EXIReader`
- `org.openexi.sax.Transmogrifier`

*Jar har har!*
