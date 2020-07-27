/**********************************************************************

  Audacity: A Digital Audio Editor

  DirectoriesPrefs.cpp

  Joshua Haberman
  James Crook


*******************************************************************//**

\class DirectoriesPrefs
\brief A PrefsPanel used to select directories.

*//*******************************************************************/

#include "../Audacity.h"
#include "DirectoriesPrefs.h"

#include <math.h>

#include <wx/defs.h>
#include <wx/intl.h>
#include <wx/log.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/dirdlg.h>
#include <wx/event.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/utils.h>

#include "../FileNames.h"
#include "../Prefs.h"
#include "../ShuttleGui.h"
#include "../widgets/AudacityMessageBox.h"

using namespace FileNames;

enum
{
   TempTextID = 1000,
   TempButtonID,

   TextsStart = 1010,
   OpenTextID,
   SaveTextID,
   ImportTextID,
   ExportTextID,
   TextsEnd,

   ButtonsStart = 1020,
   OpenButtonID,
   SaveButtonID,
   ImportButtonID,
   ExportButtonID,
   ButtonsEnd
};

BEGIN_EVENT_TABLE(DirectoriesPrefs, PrefsPanel)
   EVT_TEXT(TempTextID, DirectoriesPrefs::OnTempText)
   EVT_BUTTON(TempButtonID, DirectoriesPrefs::OnTempBrowse)
   EVT_COMMAND_RANGE(ButtonsStart, ButtonsEnd, wxEVT_BUTTON, DirectoriesPrefs::OnBrowse)
END_EVENT_TABLE()

DirectoriesPrefs::DirectoriesPrefs(wxWindow * parent, wxWindowID winid)
/* i18n-hint:  Directories, also called folders, in computer file systems */
:  PrefsPanel(parent, winid, XO("Directories")),
   mFreeSpace(NULL),
   mTempText(NULL)
{
   Populate();
}

DirectoriesPrefs::~DirectoriesPrefs()
{
}

ComponentInterfaceSymbol DirectoriesPrefs::GetSymbol()
{
   return DIRECTORIES_PREFS_PLUGIN_SYMBOL;
}

TranslatableString DirectoriesPrefs::GetDescription()
{
   return XO("Preferences for Directories");
}

wxString DirectoriesPrefs::HelpPageName()
{
   return "Directories_Preferences";
}

/// Creates the dialog and its contents.
void DirectoriesPrefs::Populate()
{
   //------------------------- Main section --------------------
   // Now construct the GUI itself.
   // Use 'eIsCreatingFromPrefs' so that the GUI is
   // initialised with values from gPrefs.
   ShuttleGui S(this, eIsCreatingFromPrefs);
   PopulateOrExchange(S);
   // ----------------------- End of main section --------------

   wxCommandEvent e;
   OnTempText(e);
}

void DirectoriesPrefs::PopulateOrExchange(ShuttleGui &S)
{
   S.SetBorder(2);
   S.StartScroller();

   S.StartStatic(XO("Temporary files directory"));
   {
      S.StartMultiColumn(3, wxEXPAND);
      {
         S.SetStretchyCol(1);

         S.Id(TempTextID);
         mTempText = S.TieTextBox(XXO("&Location:"),
                                  {PreferenceKey(Operation::Temp, PathType::_None),
                                   wxT("")},
                                  30);
         S.Id(TempButtonID).AddButton(XXO("B&rowse..."));

         S.AddPrompt(XXO("Free Space:"));
         mFreeSpace = S.Style(wxTE_READONLY).AddTextBox({}, wxT(""), 30);
         mFreeSpace->SetName(XO("Free Space").Translation());
      }
      S.EndMultiColumn();
   }
   S.EndStatic();

   S.StartStatic(XO("Default folders (\"last used\" if not specified)"));
   {
      S.StartMultiColumn(3, wxEXPAND);
      {
         S.SetStretchyCol(1);

         S.Id(OpenTextID);
         mOpenText = S.TieTextBox(XXO("&Open:"),
                                      {PreferenceKey(Operation::Open, PathType::User),
                                       wxT("")},
                                      30);
         S.Id(OpenButtonID).AddButton(XXO("Browse..."));

         S.Id(SaveTextID);
         mSaveText = S.TieTextBox(XXO("&Save:"),
                                      {PreferenceKey(Operation::Save, PathType::User),
                                       wxT("")},
                                      30);
         S.Id(SaveButtonID).AddButton(XXO("Browse..."));

         S.Id(ImportTextID);
         mImportText = S.TieTextBox(XXO("&Import:"),
                                    {PreferenceKey(Operation::Import, PathType::User),
                                     wxT("")},
                                    30);
         S.Id(ImportButtonID).AddButton(XXO("Browse..."));

         S.Id(ExportTextID);
         mExportText = S.TieTextBox(XXO("&Export:"),
                                    {PreferenceKey(Operation::Export, PathType::User),
                                     wxT("")},
                                    30);
         S.Id(ExportButtonID).AddButton(XXO("Browse..."));
      }
      S.EndMultiColumn();
   }
   S.EndStatic();

   S.EndScroller();
}

void DirectoriesPrefs::OnTempBrowse(wxCommandEvent &evt)
{
   wxString oldTemp = gPrefs->Read(PreferenceKey(Operation::Open, PathType::_None),
                                   DefaultTempDir());

   // Because we went through InitTemp() during initialisation,
   // the old temp directory name in prefs should already be OK.  Just in case there is 
   // some way we hadn't thought of for it to be not OK, 
   // we avoid prompting with it in that case and use the suggested default instead.
   if (!IsTempDirectoryNameOK(oldTemp))
   {
      oldTemp = DefaultTempDir();
   }

   wxDirDialogWrapper dlog(this,
                           XO("Choose a location to place the temporary directory"),
                           oldTemp);
   int retval = dlog.ShowModal();
   if (retval != wxID_CANCEL && !dlog.GetPath().empty())
   {
      wxFileName tmpDirPath;
      tmpDirPath.AssignDir(dlog.GetPath());

      // Append an "audacity_temp" directory to this path if necessary (the
      // default, the existing pref (as stored in the control), and any path
      // ending in a directory with the same name as what we'd add should be OK
      // already)
      wxString newDirName;
#if defined(__WXMAC__)
      newDirName = wxT("SessionData");
#elif defined(__WXMSW__) 
      // Clearing Bug 1271 residual issue.  Let's NOT have temp in the name.
      newDirName = wxT("SessionData");
#else
      newDirName = wxT(".audacity_temp");
#endif
      auto dirsInPath = tmpDirPath.GetDirs();

      // If the default temp dir or user's pref dir don't end in '/' they cause
      // wxFileName's == operator to construct a wxFileName representing a file
      // (that doesn't exist) -- hence the constructor calls
      if (tmpDirPath != wxFileName(DefaultTempDir(), wxT("")) &&
            tmpDirPath != wxFileName(mTempText->GetValue(), wxT("")) &&
            (dirsInPath.size() == 0 ||
             dirsInPath[dirsInPath.size()-1] != newDirName))
      {
         tmpDirPath.AppendDir(newDirName);
      }

      mTempText->SetValue(tmpDirPath.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR));
      OnTempText(evt);
   }
}

void DirectoriesPrefs::OnTempText(wxCommandEvent & WXUNUSED(evt))
{
   wxString Temp;
   TranslatableString label;

   if (mTempText != NULL)
   {
      Temp = mTempText->GetValue();
   }

   if (wxDirExists(Temp))
   {
      wxLongLong space;
      wxGetDiskSpace(Temp, NULL, &space);
      label = Internat::FormatSize(space);
   }
   else
   {
      label = XO("unavailable - above location doesn't exist");
   }

   if (mFreeSpace != NULL)
   {
      mFreeSpace->SetLabel(label.Translation());
      mFreeSpace->SetName(label.Translation()); // fix for bug 577 (NVDA/Narrator screen readers do not read static text in dialogs)
   }
}

void DirectoriesPrefs::OnBrowse(wxCommandEvent &evt)
{
   long id = evt.GetId() - ButtonsStart;
   wxTextCtrl *tc = (wxTextCtrl *) FindWindow(id + TextsStart);

   wxString location = tc->GetValue();

   wxDirDialogWrapper dlog(this,
                           XO("Choose a location"),
                           location);
   int retval = dlog.ShowModal();

   if (retval == wxID_CANCEL)
   {
      return;
   }

   tc->SetValue(dlog.GetPath());
}

bool DirectoriesPrefs::Validate()
{
   wxFileName Temp;
   Temp.SetPath(mTempText->GetValue());

   wxString path{Temp.GetPath()};
   if( !IsTempDirectoryNameOK( path ) ) {
      AudacityMessageBox(
         XO("Directory %s is not suitable (at risk of being cleaned out)")
            .Format( path ),
         XO("Error"),
         wxOK | wxICON_ERROR);
      return false;
   }
   if (!Temp.DirExists()) {
      int ans = AudacityMessageBox(
         XO("Directory %s does not exist. Create it?")
            .Format( path ),
         XO("New Temporary Directory"),
         wxYES_NO | wxCENTRE | wxICON_EXCLAMATION);

      if (ans != wxYES) {
         return false;
      }

      if (!Temp.Mkdir(0755, wxPATH_MKDIR_FULL)) {
         /* wxWidgets throws up a decent looking dialog */
         return false;
      }
   }
   else {
      /* If the directory already exists, make sure it is writable */
      wxLogNull logNo;
      Temp.AppendDir(wxT("canicreate"));
      path =  Temp.GetPath();
      if (!Temp.Mkdir(0755)) {
         AudacityMessageBox(
            XO("Directory %s is not writable")
               .Format( path ),
            XO("Error"),
            wxOK | wxICON_ERROR);
         return false;
      }
      Temp.Rmdir();
      Temp.RemoveLastDir();
   }

   wxFileName oldDir;
   oldDir.SetPath(FileNames::TempDir());
   if (Temp != oldDir) {
      AudacityMessageBox(
         XO(
"Changes to temporary directory will not take effect until Audacity is restarted"),
         XO("Temp Directory Update"),
         wxOK | wxCENTRE | wxICON_INFORMATION);
   }

   return true;
}

bool DirectoriesPrefs::Commit()
{
   ShuttleGui S(this, eIsSavingToPrefs);
   PopulateOrExchange(S);

   return true;
}

PrefsPanel::Factory
DirectoriesPrefsFactory()
{
   return [](wxWindow *parent, wxWindowID winid, AudacityProject *)
   {
      wxASSERT(parent); // to justify safenew
      return safenew DirectoriesPrefs(parent, winid);
   };
}

namespace
{
   PrefsPanel::Registration sAttachment
   {
      "Directories",
      DirectoriesPrefsFactory()
   };
};

