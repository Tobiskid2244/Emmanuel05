/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#ifndef __PLUMED_tools_FileBase_h
#define __PLUMED_tools_FileBase_h

#include <string>

namespace PLMD{

class Communicator;
class PlumedMain;
class Action;

/**
Base class for dealing with files.

This class just provides things which are common among OFile and IFile
*/

class FileBase{
/// Copy constructor is disabled (private and unimplemented)
  FileBase(const FileBase&);
/// Assignment operator is disabled (private and unimplemented)
  FileBase& operator=(const FileBase&);
protected:
/// Internal tool.
/// Base for IFile::Field and OFile::Field
  class FieldBase{
// everything is public to simplify usage
  public:
    std::string name;
    std::string value;
    bool constant;
    FieldBase(): constant(false){}
  };

/// file pointer
  FILE* fp;
/// communicator. NULL if not set
  Communicator* comm;
/// pointer to main plumed object. NULL if not linked
  PlumedMain* plumed;
/// pointer to corresponding action. NULL if not linked
  Action* action;
/// Control closing on destructor.
/// If true, file will not be closed in destructor
  bool cloned;
/// Private constructor.
/// In this manner one cannot instantiate a FileBase object
  FileBase();
/// Set to true when end of file is encountered
  bool eof;
/// Set to true when error is encountered
  bool err;
/// path of the opened file
  std::string path;
/// Set to true if you want flush to be heavy (close/reopen)
  bool heavyFlush;
public:
/// Link to an already open filed
  FileBase& link(FILE*);
/// Link to a PlumedMain object
/// Automatically links also the corresponding Communicator.
  FileBase& link(PlumedMain&);
/// Link to a Communicator object
  FileBase& link(Communicator&);
/// Link to an Action object.
/// Automatically links also the corresponding PlumedMain and Communicator.
  FileBase& link(Action&);
/// Flushes the file to disk
  FileBase& flush();
/// Closes the file
/// Should be used only for explicitely opened files.
  void        close();
/// Virtual destructor (allows inheritance)
  virtual ~FileBase();
/// Runs a small testcase
  static void test();
/// Check for error/eof.
  operator bool () const;
/// Set heavyFlush flag
  void setHeavyFlush(){ heavyFlush=true;}
/// Opens the file (without auto-backup)
  FileBase& open(const std::string&name,const std::string& mode);
/// Check if the file exists
  bool FileExist(const std::string& path);
/// Check if a file is open
  bool isOpen();
};


}

#endif
