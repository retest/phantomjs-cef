// Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
// All rights reserved. Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "print_handler.h"

#include "debug.h"

#include <algorithm>
#include <locale>


/*static bool compareInsensitive(const std::string& str1,const std::string& str2)
{
  
  std::string str1upper = str1;
  std::string str2upper = str2;
  
  std::transform(str1upper.begin(),str1upper.end(),str1upper.begin(),toupper);
  std::transform(str2upper.begin(),str2upper.end(),str2upper.begin(),toupper);
  
  if (str1upper != str2upper) {
     return false;
  }
  return true;
}*/

bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

/*

QPageSize::PageSizeId pageSizeIdForName(const std::string& name)
{
  // NOTE: Must keep in sync with QPageSize::PageSizeId
  static const std::string sizeNames[] = {
    // Existing Qt sizes
    "A4",
   "B5",
   "Letter",
   "Legal",
   "Executive",
   "A0",
   "A1",
   "A2",
   "A3",
   "A5",
   "A6",
   "A7",
   "A8",
   "A9",
   "B0",
   "B1",
   "B10",
   "B2",
   "B3",
   "B4",
   "B6",
   "B7",
   "B8",
   "B9",
   "C5E",
   "Comm10E",
   "DLE",
   "Folio",
   "Ledger",
   "Tabloid",
   "Custom",

  
   "A10",
   "A3Extra",
   "A4Extra",
   "A4Plus",
   "A4Small",
   "A5Extra",
   "B5Extra",

   "JisB0",
   "JisB1",
   "JisB2",
   "JisB3",
   "JisB4",
   "JisB5",
   "JisB6",
   "JisB7",
   "JisB8",
   "JisB9",
   "JisB10",
    // AnsiA = QStringLiteral("Letter",
    // AnsiB = QStringLiteral("Ledger",
    "AnsiC",
    "AnsiD",
    "AnsiE",
    "LegalExtra",
    "LetterExtra",
    "LetterPlus",
    "LetterSmall",
    "TabloidExtra",

    "ArchA",
    "ArchB",
    "ArchC",
    "ArchD",
    "ArchE",

    "Imperial7x9",
    "Imperial8x10",
    "Imperial9x11",
    "Imperial9x12",
    "Imperial10x11",
    "Imperial10x13",
    "Imperial10x14",
    "Imperial12x11",
    "Imperial15x11",

    "ExecutiveStandard",
    "Note",
    "Quarto",
    "Statement",
    "SuperA",
    "SuperB",
    "Postcard",
    "DoublePostcard",
    "Prc16K",
    "Prc32K",
    "Prc32KBig",

    "FanFoldUS",
    "FanFoldGerman",
    "FanFoldGermanLegal",

    "EnvelopeB4",
    "EnvelopeB5",
    "EnvelopeB6",
    "EnvelopeC0",
    "EnvelopeC1",
    "EnvelopeC2",
    "EnvelopeC3",
    "EnvelopeC4",
    
    "EnvelopeC6",
    "EnvelopeC65",
    "EnvelopeC7",
    

   "Envelope9",
   "Envelope11",
   "Envelope12",
   "Envelope14",
   "EnvelopeMonarch",
   "EnvelopePersonal",

   "EnvelopeChou3",
   "EnvelopeChou4",
   "EnvelopeInvite",
   "EnvelopeItalian",
   "EnvelopeKaku2",
   "EnvelopeKaku3",
   "EnvelopePrc1",
   "EnvelopePrc2",
   "EnvelopePrc3",
   "EnvelopePrc4",
   "EnvelopePrc5",
   "EnvelopePrc6",
   "EnvelopePrc7",
   "EnvelopePrc8",
   "EnvelopePrc9",
   "EnvelopePrc10",
   "EnvelopeYou4",
  };
  auto it = std::find_if(std::begin(sizeNames), std::end(sizeNames), [name] (const std::string& size) {
    return compareInsensitive(name,size);
  });
  if (it != std::end(sizeNames)) {
    return static_cast<QPageSize::PageSizeId>(std::distance(std::begin(sizeNames), it));
  }


  if(compareInsensitive(name,"AnsiA"))
    return QPageSize::AnsiA;
  if (compareInsensitive(name,"AnsiB"))
    return QPageSize::AnsiB;
  if (compareInsensitive(name,"EnvelopeC5"))
    return QPageSize::EnvelopeC5;
  if (compareInsensitive(name,"EnvelopeDL"))
    return QPageSize::EnvelopeDL;
  if (compareInsensitive(name,"Envelope10"))
    return QPageSize::Envelope10;

  qCWarning(print) << "Unknown page size:" << name << "defaulting to A4.";
  return QPageSize::A4;
}

QPageSize pageSizeForName(const std::string& name)
{
  return QPageSize(pageSizeIdForName(name));
}
*/
#define PHANTOMJS_PDF_DPI 72.0f

struct UnitConversion
{
    UnitConversion(const std::string& unit, float factor)
        : unit(unit)
        , factor(factor)
    {}
    std::string unit;
    float factor;
};

float stringToPointSize(const std::string& string)
{
  static const UnitConversion units[] = {
    { "mm", 72.0f / 25.4f },
    { "cm", 72.0f / 2.54f },
    { "in", 72.0f },
    { "px", 72.0f / PHANTOMJS_PDF_DPI },
  };

  for (uint i = 0; i < sizeof(units) / sizeof(units[0]); ++i) {
    if(hasEnding(string,units[i].unit))
    {
      return std::stof(string.substr(0, string.size() - units[i].unit.size()))
              * units[i].factor;
    }
    
  }
  return std::stof(string);
}

int stringToMillimeter(const std::string& string)
{
  return round(stringToPointSize(string) / PHANTOMJS_PDF_DPI * 25.4f);
}

CefSize PrintHandler::GetPdfPaperSize(int device_units_per_inch)
{
  // this is just a default, we configure the size via CefPdfPrintSettings in handler.cpp
  /*QPageSize page(QPageSize::A4);
  auto rect = page.rectPixels(device_units_per_inch);*/
  return CefSize(2480, 3580);
}

bool PrintHandler::OnPrintDialog(bool has_selection, CefRefPtr<CefPrintDialogCallback> callback)
{
  qCDebug(print) << has_selection;
  return false;
}

bool PrintHandler::OnPrintJob(const CefString& document_name, const CefString& pdf_file_path, CefRefPtr<CefPrintJobCallback> callback)
{
  qCDebug(print) << document_name << pdf_file_path;
  return false;
}

void PrintHandler::OnPrintReset()
{
  qCDebug(print) << "!";
}

void PrintHandler::OnPrintSettings(CefRefPtr<CefPrintSettings> settings, bool get_defaults)
{
  qCDebug(print) << get_defaults;
}

#if CEF_COMMIT_NUMBER > 1333 // TODO: find correct commit number that adds this
void PrintHandler::OnPrintStart(CefRefPtr<CefBrowser> browser)
{
  qCDebug(print) << "!";
}
#endif
