//===- xerces/parsers/EEAwareParserConfiguration.java ---------------===//
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

package org.apache.xerces.parsers;

import org.apache.xerces.impl.Constants;
import org.apache.xerces.impl.ExtendedNSSupportConfig;
import org.apache.xerces.impl.XML11DTDScannerImpl;
import org.apache.xerces.impl.XML11DocumentScannerImpl;
import org.apache.xerces.impl.XML11NSDocumentScannerImpl;
import org.apache.xerces.impl.XMLDTDScannerImpl;
import org.apache.xerces.impl.XMLDocumentScannerImpl;
import org.apache.xerces.impl.XMLEntityHandler;
import org.apache.xerces.impl.XMLEntityManager;
import org.apache.xerces.impl.XMLErrorReporter;
import org.apache.xerces.impl.XMLNSDocumentScannerImpl;
import org.apache.xerces.impl.XMLVersionDetector;
import org.apache.xerces.impl.dtd.XML11DTDProcessor;
import org.apache.xerces.impl.dtd.XML11DTDValidator;
import org.apache.xerces.impl.dtd.XML11NSDTDValidator;
import org.apache.xerces.impl.dtd.XMLDTDProcessor;
import org.apache.xerces.impl.dtd.XMLDTDValidator;
import org.apache.xerces.impl.dtd.XMLNSDTDValidator;
import org.apache.xerces.impl.msg.XMLMessageFormatter;
import org.apache.xerces.impl.xs.XMLSchemaValidator;
import org.apache.xerces.impl.xs.XSMessageFormatter;
import org.apache.xerces.parsers.XIncludeAwareParserConfiguration;
import org.apache.xerces.util.NamespaceSupport;
import org.apache.xerces.util.SymbolTable;
import org.apache.xerces.xinclude.XIncludeHandler;
import org.apache.xerces.xinclude.XIncludeNamespaceSupport;
import org.apache.xerces.xni.NamespaceContext;
import org.apache.xerces.xni.XMLDTDHandler;
import org.apache.xerces.xni.XMLDocumentHandler;
import org.apache.xerces.xni.grammars.XMLGrammarPool;
import org.apache.xerces.xni.parser.XMLComponent;
import org.apache.xerces.xni.parser.XMLComponentManager;
import org.apache.xerces.xni.parser.XMLConfigurationException;
import org.apache.xerces.xni.parser.XMLDTDScanner;
import org.apache.xerces.xni.parser.XMLDocumentScanner;
import org.apache.xerces.xni.parser.XMLDocumentSource;
import org.apache.xerces.xni.parser.XMLEntityResolver;
import org.apache.xerces.xni.parser.XMLErrorHandler;
import org.apache.xerces.xni.parser.XMLInputSource;

import org.apache.xerces.impl.XMLEEDocumentScanner;
import org.apache.xerces.impl.XMLEENSDocumentScanner;
import org.apache.xerces.util.ConfigReflector;
import org.exicpp.util.XConstants;

/**
 * This class is the configuration used to parse XML 1.0 and XML 1.1 documents
 * and provides support for XInclude and embedded entities.
 */
public class EEAwareParserConfiguration extends XIncludeAwareParserConfiguration {
  /**
   * Feature identifier: allow notation and unparsed entity events to be sent
   * out of order.
   */
  protected static final String ALLOW_UE_AND_NOTATION_EVENTS =
      Constants.SAX_FEATURE_PREFIX +
      Constants.ALLOW_DTD_EVENTS_AFTER_ENDDTD_FEATURE;
  
  /** Feature identifier: notify built-in refereces. */
  private static final String NOTIFY_BUILTIN_REFS =
    Constants.XERCES_FEATURE_PREFIX + Constants.NOTIFY_BUILTIN_REFS_FEATURE;

  /** New Feature identifier: Embedded escape sequences */
  protected static final String ALLOW_WEIRD_ATTRS =
      XConstants.EXICPP_FEATURE_PREFIX + XConstants.ALLOW_WEIRD_ATTRS;

  /** New Feature identifier: Embedded escape sequences */
  protected static final String EMBED_ESCAPE_SEQUENCES =
      XConstants.EXICPP_FEATURE_PREFIX + XConstants.EMBED_ESCAPE_SEQUENCES;
  
  // Properties

  /** Property identifier: symbol table. */
  protected static final String SYMBOL_TABLE =
    Constants.XERCES_PROPERTY_PREFIX +
    Constants.SYMBOL_TABLE_PROPERTY;

  /** Property identifier: XML grammar pool. */
  protected static final String XMLGRAMMAR_POOL =
    Constants.XERCES_PROPERTY_PREFIX +
    Constants.XMLGRAMMAR_POOL_PROPERTY;
  
  /** New Property identifier: DTD handler. */
  protected static final String DTD_HANDLER =
      XConstants.EXICPP_PROPERTY_PREFIX + XConstants.DTD_HANDLER_PROPERTY;

  /** New Property identifier: Escape handler. */
  protected static final String ESCAPE_HANDLER =
      XConstants.EXICPP_PROPERTY_PREFIX + XConstants.ESCAPE_HANDLER_PROPERTY;
  
  /** New Property identifier: Property reflector. */
  protected static final String CONFIG_REFLECTOR = ConfigReflector.CONFIG_REFLECTOR;

  //
  // Components
  //

  /** Embedded Escape XMLDocumentScanner */
  protected XMLDocumentScanner fEECurrentScanner;

  /** Embedded Escape XMLNSDocumentScanner */
  protected XMLEENSDocumentScanner fEENamespaceScanner;

  /** Embedded Escape XMLDocumentScanner */
  protected XMLEEDocumentScanner fEENonNSScanner;

  /** Flag indicating whether weird attributes are enabled. */
  protected boolean fWeirdAttrsEnabled = false;

  /** Flag indicating whether embedded escape sequences are enabled. */
  protected boolean fEmbeddedEscapesEnabled = false;

  /// Error states

  /** Flag indicating whether xml 1.1 embedded escapes error has been printed. */
  private boolean hpe_XML11EEE = false;

  /** Flag indicating whether xinclude embedded escapes error has been printed. */
  private boolean hpe_XIncludeEEE = false;

  /** Flag indicating whether xinclude embedded escapes error has been printed. */
  private boolean hpe_WeirdAttrsE = false;

  /** Default constructor. */
  public EEAwareParserConfiguration() {
    this(null, null, null);
  } // <init>()

  /**
   * Constructs a parser configuration using the specified symbol table.
   *
   * @param symbolTable The symbol table to use.
   */
  public EEAwareParserConfiguration(SymbolTable symbolTable) {
    this(symbolTable, null, null);
  } // <init>(SymbolTable)

  /**
   * Constructs a parser configuration using the specified symbol table and
   * grammar pool.
   * <p>
   *
   * @param symbolTable The symbol table to use.
   * @param grammarPool The grammar pool to use.
   */
  public EEAwareParserConfiguration(SymbolTable symbolTable,
                                    XMLGrammarPool grammarPool) {
    this(symbolTable, grammarPool, null);
  } // <init>(SymbolTable,XMLGrammarPool)

  /**
   * Constructs a parser configuration using the specified symbol table,
   * grammar pool, and parent settings.
   * <p>
   *
   * @param symbolTable    The symbol table to use.
   * @param grammarPool    The grammar pool to use.
   * @param parentSettings The parent settings.
   */
  public EEAwareParserConfiguration(SymbolTable symbolTable,
                                    XMLGrammarPool grammarPool,
                                    XMLComponentManager parentSettings) {
    super(symbolTable, grammarPool, parentSettings);

    final String[] recognizedFeatures = {
      ALLOW_UE_AND_NOTATION_EVENTS,
      ALLOW_WEIRD_ATTRS,
      EMBED_ESCAPE_SEQUENCES,
      NOTIFY_BUILTIN_REFS,
    };
    addRecognizedFeatures(recognizedFeatures);

    setFeature(ALLOW_WEIRD_ATTRS, false);
    setFeature(EMBED_ESCAPE_SEQUENCES, false);
    setFeature(NOTIFY_BUILTIN_REFS, true);

    // add default recognized properties
    final String[] recognizedProperties = {
      DTD_HANDLER,
      ESCAPE_HANDLER,
      CONFIG_REFLECTOR,
      SYMBOL_TABLE,
      XMLGRAMMAR_POOL,
    };
    addRecognizedProperties(recognizedProperties);

    if (symbolTable != null)
      setProperty(SYMBOL_TABLE, symbolTable);
    if (grammarPool != null)
      setProperty(XMLGRAMMAR_POOL, grammarPool);

    fEENamespaceScanner = new XMLEENSDocumentScanner();
    fEECurrentScanner = fEENamespaceScanner;
    setProperty(ESCAPE_HANDLER, fEENamespaceScanner);
    addComponent((XMLComponent) fEENamespaceScanner);
  }

  protected void configureXInclude() {
    if (fXIncludeEnabled) {
      // If the XInclude handler was not in the pipeline insert it.
      if (fXIncludeHandler == null) {
        fXIncludeHandler = new XIncludeHandler();
        // add XInclude component
        setProperty(XINCLUDE_HANDLER, fXIncludeHandler);
        addCommonComponent(fXIncludeHandler);
        fXIncludeHandler.reset(this);
      }
      // Setup NamespaceContext
      if (fCurrentNSContext != fXIncludeNSContext) {
        if (fXIncludeNSContext == null) {
          fXIncludeNSContext = new XIncludeNamespaceSupport();
        }
        fCurrentNSContext = fXIncludeNSContext;
        setProperty(NAMESPACE_CONTEXT, fXIncludeNSContext);
      }
    } else {
      // Setup NamespaceContext
      if (fCurrentNSContext != fNonXIncludeNSContext) {
        fCurrentNSContext = fNonXIncludeNSContext;
        setProperty(NAMESPACE_CONTEXT, fNonXIncludeNSContext);
      }
    }
  }

  protected void configureWeirdAttr() {
    if (fCurrentScanner instanceof ExtendedNSSupportConfig nsctx) {
      nsctx.setAllowWeirdColonInAttributes(fWeirdAttrsEnabled);
    } else if (fFeatures.get(NAMESPACES) == Boolean.TRUE && fWeirdAttrsEnabled) {
      if (!hpe_WeirdAttrsE) {
        System.err.format("Feature %s has not been set up for scanner type %s",
                          XConstants.ALLOW_WEIRD_ATTRS,
                          fCurrentScanner.getClass().getName());
        hpe_WeirdAttrsE = true;
      }
    }
  }

  /** Configures the pipeline. */
  @Override
  protected void configurePipeline() {
    if (!fEmbeddedEscapesEnabled) {
      super.configurePipeline();
      configureWeirdAttr();
      return;
    } else {
      configurePipeline0();
      configureWeirdAttr();
    }
    
    configureXInclude();
    if (fXIncludeEnabled) {
      // configure DTD pipeline
      fDTDScanner.setDTDHandler(fDTDProcessor);
      fDTDProcessor.setDTDSource(fDTDScanner);
      fDTDProcessor.setDTDHandler(fXIncludeHandler);
      fXIncludeHandler.setDTDSource(fDTDProcessor);
      fXIncludeHandler.setDTDHandler(fDTDHandler);
      if (fDTDHandler != null) {
        fDTDHandler.setDTDSource(fXIncludeHandler);
      }

      // configure XML document pipeline: insert after DTDValidator and
      // before XML Schema validator
      XMLDocumentSource prev = null;
      if (fFeatures.get(XMLSCHEMA_VALIDATION) == Boolean.TRUE) {
        // we don't have to worry about fSchemaValidator being null, since
        // super.configurePipeline() instantiated it if the feature was set
        prev = fSchemaValidator.getDocumentSource();
      }
      // Otherwise, insert after the last component in the pipeline
      else {
        prev = fLastComponent;
        fLastComponent = fXIncludeHandler;
      }

      XMLDocumentHandler next = prev.getDocumentHandler();
      prev.setDocumentHandler(fXIncludeHandler);
      fXIncludeHandler.setDocumentSource(prev);
      if (next != null) {
        fXIncludeHandler.setDocumentHandler(next);
        next.setDocumentSource(fXIncludeHandler);
      }
    }
  } // configurePipeline()

  @Override
  protected void configureXML11Pipeline() {
    if (fEmbeddedEscapesEnabled) {
      if (!hpe_XML11EEE) {
        System.err.format("Feature %s has not been implemented for xml 1.1%n",
                          XConstants.EMBED_ESCAPE_SEQUENCES);
        hpe_XML11EEE = true;
      }
    }
    super.configureXML11Pipeline();
    configureWeirdAttr();

    /*
    configureXInclude();
    if (fXIncludeEnabled) {
      // configure XML 1.1. DTD pipeline
      fXML11DTDScanner.setDTDHandler(fXML11DTDProcessor);
      fXML11DTDProcessor.setDTDSource(fXML11DTDScanner);
      fXML11DTDProcessor.setDTDHandler(fXIncludeHandler);
      fXIncludeHandler.setDTDSource(fXML11DTDProcessor);
      fXIncludeHandler.setDTDHandler(fDTDHandler);
      if (fDTDHandler != null) {
        fDTDHandler.setDTDSource(fXIncludeHandler);
      }

      // configure XML document pipeline: insert after DTDValidator and
      // before XML Schema validator
      XMLDocumentSource prev = null;
      if (fFeatures.get(XMLSCHEMA_VALIDATION) == Boolean.TRUE) {
        // we don't have to worry about fSchemaValidator being null, since
        // super.configurePipeline() instantiated it if the feature was set
        prev = fSchemaValidator.getDocumentSource();
      }
      // Otherwise, insert after the last component in the pipeline
      else {
        prev = fLastComponent;
        fLastComponent = fXIncludeHandler;
      }

      XMLDocumentHandler next = prev.getDocumentHandler();
      prev.setDocumentHandler(fXIncludeHandler);
      fXIncludeHandler.setDocumentSource(prev);
      if (next != null) {
        fXIncludeHandler.setDocumentHandler(next);
        next.setDocumentSource(fXIncludeHandler);
      }
    }
    */
  } // configureXML11Pipeline()

  /** Configures the pipeline. */
  protected void configurePipeline0() {
    assert fEmbeddedEscapesEnabled;
    if (fCurrentDVFactory != fDatatypeValidatorFactory) {
      fCurrentDVFactory = fDatatypeValidatorFactory;
      // use XML 1.0 datatype library
      setProperty(DATATYPE_VALIDATOR_FACTORY, fCurrentDVFactory);
    }

    // setup DTD pipeline
    if (fCurrentDTDScanner != fDTDScanner) {
      fCurrentDTDScanner = fDTDScanner;
      setProperty(DTD_SCANNER, fCurrentDTDScanner);
      setProperty(DTD_PROCESSOR, fDTDProcessor);
    }
    fDTDScanner.setDTDHandler(fDTDProcessor);
    fDTDProcessor.setDTDSource(fDTDScanner);
    fDTDProcessor.setDTDHandler(fDTDHandler);
    if (fDTDHandler != null) {
      fDTDHandler.setDTDSource(fDTDProcessor);
    }

    fDTDScanner.setDTDContentModelHandler(fDTDProcessor);
    fDTDProcessor.setDTDContentModelSource(fDTDScanner);
    fDTDProcessor.setDTDContentModelHandler(fDTDContentModelHandler);
    if (fDTDContentModelHandler != null) {
      fDTDContentModelHandler.setDTDContentModelSource(fDTDProcessor);
    }

    // setup document pipeline
    if (fFeatures.get(NAMESPACES) == Boolean.TRUE) {
      if (fEECurrentScanner != fEENamespaceScanner) {
        fEECurrentScanner = fEENamespaceScanner;
        setProperty(ESCAPE_HANDLER, fEENamespaceScanner);
      }
      if (fCurrentScanner != fEENamespaceScanner) {
        fCurrentScanner = fEENamespaceScanner;
        setProperty(DOCUMENT_SCANNER, fEENamespaceScanner);
        setProperty(DTD_VALIDATOR, fDTDValidator);
      }
      fEENamespaceScanner.setDTDValidator(fDTDValidator);
      fEENamespaceScanner.setDocumentHandler(fDTDValidator);
      fDTDValidator.setDocumentSource(fEENamespaceScanner);
      fDTDValidator.setDocumentHandler(fDocumentHandler);
      if (fDocumentHandler != null) {
        fDocumentHandler.setDocumentSource(fDTDValidator);
      }
      fLastComponent = fDTDValidator;
    } else {
      // create components
      if (fEENonNSScanner == null) {
        // TODO: Handle creating fNonNSScanner?
        fEENonNSScanner = new XMLEEDocumentScanner();
        // add components
        addComponent((XMLComponent)fEENonNSScanner);
      }
      if (fNonNSDTDValidator == null) {
        fNonNSDTDValidator = new XMLDTDValidator();
        // add components
        addComponent((XMLComponent)fNonNSDTDValidator);
      }
      if (fEECurrentScanner != fEENonNSScanner) {
        fEECurrentScanner = fEENonNSScanner;
        setProperty(ESCAPE_HANDLER, fEENonNSScanner);
      }
      if (fCurrentScanner != fEENonNSScanner) {
        fCurrentScanner = fEENonNSScanner;
        setProperty(ESCAPE_HANDLER, fEENonNSScanner);
        setProperty(DOCUMENT_SCANNER, fEENonNSScanner);
        setProperty(DTD_VALIDATOR, fNonNSDTDValidator);
      }

      fEENonNSScanner.setDocumentHandler(fNonNSDTDValidator);
      fNonNSDTDValidator.setDocumentSource(fEENonNSScanner);
      fNonNSDTDValidator.setDocumentHandler(fDocumentHandler);
      if (fDocumentHandler != null) {
        fDocumentHandler.setDocumentSource(fNonNSDTDValidator);
      }
      fLastComponent = fNonNSDTDValidator;
    }

    // add XML Schema validator if needed
    if (fFeatures.get(XMLSCHEMA_VALIDATION) == Boolean.TRUE) {
      // If schema validator was not in the pipeline insert it.
      if (fSchemaValidator == null) {
        fSchemaValidator = new XMLSchemaValidator();
        // add schema component
        setProperty(SCHEMA_VALIDATOR, fSchemaValidator);
        addCommonComponent(fSchemaValidator);
        fSchemaValidator.reset(this);
        // add schema message formatter
        if (fErrorReporter.getMessageFormatter(
                XSMessageFormatter.SCHEMA_DOMAIN) == null) {
          XSMessageFormatter xmft = new XSMessageFormatter();
          fErrorReporter.putMessageFormatter(XSMessageFormatter.SCHEMA_DOMAIN,
                                             xmft);
        }
      }
      fLastComponent.setDocumentHandler(fSchemaValidator);
      fSchemaValidator.setDocumentSource(fLastComponent);
      fSchemaValidator.setDocumentHandler(fDocumentHandler);
      if (fDocumentHandler != null) {
        fDocumentHandler.setDocumentSource(fSchemaValidator);
      }
      fLastComponent = fSchemaValidator;
    }
  } // configurePipeline0()

  @Override
  public boolean getFeature(String featureId) throws XMLConfigurationException {
    if (featureId.equals(EMBED_ESCAPE_SEQUENCES)) {
      return fEmbeddedEscapesEnabled;
    } else if (featureId.equals(ALLOW_WEIRD_ATTRS)) {
      return fWeirdAttrsEnabled;
    }
    return super.getFeature0(featureId);
  } // getFeature(String):boolean

  @Override
  public void setFeature(String featureId, boolean state)
      throws XMLConfigurationException {
    if (featureId.equals(EMBED_ESCAPE_SEQUENCES)) {
      fEmbeddedEscapesEnabled = state;
      fConfigUpdated = true;
      return;
    } else if (featureId.equals(ALLOW_WEIRD_ATTRS)) {
      fWeirdAttrsEnabled = state;
      fConfigUpdated = true;
      return;
    }
    super.setFeature(featureId, state);
  }

  @Override
  public Object getProperty(String featureId) throws XMLConfigurationException {
    if (featureId.equals(DTD_HANDLER)) {
      return fEmbeddedEscapesEnabled;
    }
    return super.getProperty(featureId);
  } // getFeature(String):boolean

  @Override
  public void setProperty(String featureId, Object value) throws XMLConfigurationException {
    if (featureId.equals(DTD_HANDLER)) {
      if (value == null) {
        if (fDTDHandler == null)
          return;
        // Reset to the default
        setDTDHandler(null);
        fConfigUpdated = true;
        return;
      }

      if (value instanceof XMLDTDHandler dtdHandler) {
        if (fDTDHandler == dtdHandler)
          return;
        // Change to new handler
        setDTDHandler(dtdHandler);
        fConfigUpdated = true;
        return;
      }

      final short type = XMLConfigurationException.NOT_SUPPORTED;
      throw new XMLConfigurationException(type, featureId);
    }
    super.setProperty(featureId, value);
  }
}
