/*
** Name:        wxsqlite3.cpp
** Purpose:     Implementation of wxSQLite3 classes
** Author:      Ulrich Telle
** Created:     2005-07-06
** Copyright:   (c) 2005-2025 Ulrich Telle and the wxSQLite3 contributors
** SPDX-License-Identifier: LGPL-3.0+ WITH WxWindows-exception-3.1
*/

/// \file wxsqlite3.cpp Implementation of the wxSQLite3 class

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma implementation "wxsqlite3.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

//#include <vld.h>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/dynarray.h"
#include "wx/regex.h"
#include "wx/thread.h"

#include "wx/wxsqlite3.h"
#include "wx/wxsqlite3opt.h"

// Suppress some Visual C++ warnings regarding the default constructor
// for a C struct used only in SQLite modules
#ifdef __VISUALC__
#pragma warning (disable:4510)
#pragma warning (disable:4610)
#endif

#include "sqlite3mc_config.h"
#include "sqlite3mc_amalgamation.h"

// Check for minimal required SQLite version
#if SQLITE_VERSION_NUMBER < 3032000
#error SQLite version 3.32.0 or higher required.
#endif

typedef int (*sqlite3_xauth)(void*,int,const char*,const char*,const char*,const char*);

// Local declaration of the ExecAuthorizer function
// to avoid dependency on user authentication enabled or not
static int wxSQLite3FunctionContextExecAuthorizer(void* func, int type,
  const char* arg1, const char* arg2,
  const char* arg3, const char* arg4
  );

// Error messages

#if wxCHECK_VERSION(2,9,0)
typedef char err_char_t;
#else
typedef wxChar err_char_t;
#endif

const err_char_t* wxERRMSG_NODB = wxTRANSLATE("No Database opened");
const err_char_t* wxERRMSG_NOSTMT = wxTRANSLATE("Statement not accessible");
const err_char_t* wxERRMSG_NOMEM = wxTRANSLATE("Out of memory");
const err_char_t* wxERRMSG_DECODE = wxTRANSLATE("Cannot decode binary");
const err_char_t* wxERRMSG_INVALID_INDEX = wxTRANSLATE("Invalid field index");
const err_char_t* wxERRMSG_INVALID_NAME = wxTRANSLATE("Invalid field name");
const err_char_t* wxERRMSG_INVALID_ROW = wxTRANSLATE("Invalid row index");
const err_char_t* wxERRMSG_INVALID_QUERY = wxTRANSLATE("Invalid scalar query");
const err_char_t* wxERRMSG_INVALID_BLOB = wxTRANSLATE("Invalid BLOB handle");

const err_char_t* wxERRMSG_NORESULT = wxTRANSLATE("Null Results pointer");
const err_char_t* wxERRMSG_BIND_STR = wxTRANSLATE("Error binding string param");
const err_char_t* wxERRMSG_BIND_INT = wxTRANSLATE("Error binding int param");
const err_char_t* wxERRMSG_BIND_INT64 = wxTRANSLATE("Error binding int64 param");
const err_char_t* wxERRMSG_BIND_DBL = wxTRANSLATE("Error binding double param");
const err_char_t* wxERRMSG_BIND_BLOB = wxTRANSLATE("Error binding blob param");
const err_char_t* wxERRMSG_BIND_DATETIME = wxTRANSLATE("Error binding date/time param");
const err_char_t* wxERRMSG_BIND_NULL = wxTRANSLATE("Error binding NULL param");
const err_char_t* wxERRMSG_BIND_ZEROBLOB = wxTRANSLATE("Error binding zero blob param");
const err_char_t* wxERRMSG_BIND_POINTER = wxTRANSLATE("Error binding pointer param");
const err_char_t* wxERRMSG_BIND_CLEAR = wxTRANSLATE("Error clearing bindings");

const err_char_t* wxERRMSG_NOMETADATA = wxTRANSLATE("Meta data support not available");
const err_char_t* wxERRMSG_NOCODEC = wxTRANSLATE("Encryption support not available");
const err_char_t* wxERRMSG_NOLOADEXT = wxTRANSLATE("Loadable extension support not available");
const err_char_t* wxERRMSG_NOINCBLOB = wxTRANSLATE("Incremental BLOB support not available");
const err_char_t* wxERRMSG_NOBLOBREBIND = wxTRANSLATE("Rebind BLOB support not available");
const err_char_t* wxERRMSG_NOPOINTER = wxTRANSLATE("Pointer parameter support not available");
const err_char_t* wxERRMSG_NOSAVEPOINT = wxTRANSLATE("Savepoint support not available");
const err_char_t* wxERRMSG_NOBACKUP = wxTRANSLATE("Backup/restore support not available");
const err_char_t* wxERRMSG_NOWAL = wxTRANSLATE("Write Ahead Log support not available");
const err_char_t* wxERRMSG_NOCOLLECTIONS = wxTRANSLATE("Named collection support not available");

const err_char_t* wxERRMSG_SHARED_CACHE = wxTRANSLATE("Setting SQLite shared cache mode failed");

const err_char_t* wxERRMSG_INITIALIZE = wxTRANSLATE("Initialization of SQLite failed");
const err_char_t* wxERRMSG_SHUTDOWN = wxTRANSLATE("Shutdown of SQLite failed");
const err_char_t* wxERRMSG_TEMPDIR = wxTRANSLATE("Setting temporary directory failed");

const err_char_t* wxERRMSG_SOURCEDB_BUSY = wxTRANSLATE("Source database is busy");
const err_char_t* wxERRMSG_DBOPEN_FAILED = wxTRANSLATE("Database open failed");
const err_char_t* wxERRMSG_DBCLOSE_FAILED = wxTRANSLATE("Database close failed");
const err_char_t* wxERRMSG_DBASSIGN_FAILED = wxTRANSLATE("Database assignment failed");
const err_char_t* wxERRMSG_FINALIZE_FAILED = wxTRANSLATE("Finalize failed");

const err_char_t* wxERRMSG_CIPHER_APPLY_FAILED = wxTRANSLATE("Application of cipher failed");
const err_char_t* wxERRMSG_CIPHER_NOT_SUPPORTED = wxTRANSLATE("Cipher not supported");

const err_char_t* wxERRMSG_INVALID_COLLECTION = wxTRANSLATE("Collection instance not properly initialized");

const err_char_t* wxERRMSG_SCHEMANAME_UNKNOWN = wxTRANSLATE("Schema name unknown");

const err_char_t* wxERRMSG_DBCONFIG_OPTION_UNKNOWN = wxTRANSLATE("Database configuration option unknown");

static const char* LocalMakePointerTypeCopy(wxArrayPtrVoid& ptrTypes, const wxString& pointerType)
{
  // Convert pointer type to char*
  wxCharBuffer strPointerType = pointerType.ToUTF8();
  const char* localPointerType = strPointerType;

  // Check whether pointer type was already registered
  char* ptrTypeCopy = NULL;
  size_t nPtrTypes = ptrTypes.GetCount();
  for (size_t j = 0; (ptrTypeCopy == NULL) && (j < nPtrTypes); ++j)
  {
    if (strcmp(localPointerType, (char*) ptrTypes[j]) == 0)
    {
      ptrTypeCopy = (char*) ptrTypes[j];
    }
  }

  // Create copy of pointer type if not found
  if (ptrTypeCopy == NULL)
  {
    int n = (int) strlen(localPointerType);
    ptrTypeCopy = (char*) sqlite3_malloc(n + 1);
    if (ptrTypeCopy != NULL)
    {
      strcpy(ptrTypeCopy, localPointerType);
      ptrTypes.Add(ptrTypeCopy);
    }
  }

  return (const char*) ptrTypeCopy;
}

// Critical sections are used to make access to it thread safe if necessary.
#if wxUSE_THREADS
static wxCriticalSection gs_csDatabase;
static wxCriticalSection gs_csStatment;
static wxCriticalSection gs_csBlob;
#endif

/// Reference counted database object (internal)
class wxSQLite3DatabaseReference
{
public:
  /// Default constructor
  wxSQLite3DatabaseReference(sqlite3* db = NULL)
    : m_db(db)
  {
    m_db = db;
    if (m_db != NULL)
    {
      m_isValid = true;
      m_refCount = 1;
    }
    else
    {
      m_isValid = false;
      m_refCount = 0;
    }
  }

  /// Default destructor
  virtual ~wxSQLite3DatabaseReference()
  {
  }

private:
  /// Thread safe increment of the reference count
  int IncrementRefCount()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csDatabase);
#endif
    return ++m_refCount;
  }

  /// Invalidate instance
  void Invalidate()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csDatabase);
#endif
    m_isValid = false;
  }

  /// Thread safe decrement of the reference count
  int DecrementRefCount()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csDatabase);
#endif
    if (m_refCount > 0) --m_refCount;
    return m_refCount;
  }

  sqlite3* m_db;        ///< SQLite database reference
  int      m_refCount;  ///< Reference count
  bool     m_isValid;   ///< SQLite database reference is valid

  friend class WXDLLIMPEXP_FWD_SQLITE3 wxSQLite3Database;
  friend class WXDLLIMPEXP_FWD_SQLITE3 wxSQLite3ResultSet;
  friend class WXDLLIMPEXP_FWD_SQLITE3 wxSQLite3Statement;
  friend class WXDLLIMPEXP_FWD_SQLITE3 wxSQLite3Blob;
};

/// Reference counted statement object (internal)
class wxSQLite3StatementReference
{
public:
  /// Default constructor
  wxSQLite3StatementReference(sqlite3_stmt* stmt = NULL)
    : m_stmt(stmt), m_ptrTypes(NULL)
  {
    m_stmt = stmt;
    if (m_stmt != NULL)
    {
      m_isValid = true;
      m_refCount = 0;
    }
    else
    {
      m_isValid = false;
      m_refCount = 0;
    }
  }

  /// Default destructor
  virtual ~wxSQLite3StatementReference()
  {
    if (m_ptrTypes != NULL)
    {
      size_t n = m_ptrTypes->GetCount();
      for (size_t j = 0; j < n; ++j)
      {
        sqlite3_free((*m_ptrTypes)[j]);
      }
      delete m_ptrTypes;
    }
  }

private:
  /// Thread safe increment of the reference count
  int IncrementRefCount()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csStatment);
#endif
    return ++m_refCount;
  }

  /// Thread safe decrement of the reference count
  int DecrementRefCount()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csStatment);
#endif
    if (m_refCount > 0) --m_refCount;
    return m_refCount;
  }

  /// Invalidate instance
  void Invalidate()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csStatment);
#endif
    m_isValid = false;
  }

  /// Manage pointer types
  const char* MakePointerTypeCopy(const wxString& pointerType)
  {
    // Allocate pointer type array if necessary
    if (m_ptrTypes == NULL)
    {
      m_ptrTypes = new wxArrayPtrVoid();
    }

    // Convert pointer type to char*
    return LocalMakePointerTypeCopy(*m_ptrTypes, pointerType);
  }

  sqlite3_stmt*   m_stmt;           ///< SQLite statement reference
  int             m_refCount;       ///< Reference count
  bool            m_isValid;        ///< SQLite statement reference is valid
  wxArrayPtrVoid* m_ptrTypes;       ///< Keeping track of pointer types

  friend class WXDLLIMPEXP_FWD_SQLITE3 wxSQLite3ResultSet;
  friend class WXDLLIMPEXP_FWD_SQLITE3 wxSQLite3Statement;
};

/// Reference counted blob object (internal)
class wxSQLite3BlobReference
{
public:
  /// Default constructor
  wxSQLite3BlobReference(sqlite3_blob* blob = NULL)
    : m_blob(blob)
  {
    m_blob = blob;
    if (m_blob != NULL)
    {
      m_isValid = true;
      m_refCount = 0;
    }
    else
    {
      m_isValid = false;
      m_refCount = 0;
    }
  }

  /// Default destructor
  virtual ~wxSQLite3BlobReference()
  {
  }

private:
  /// Thread safe increment of the reference count
  int IncrementRefCount()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csBlob);
#endif
    return ++m_refCount;
  }

  /// Thread safe decrement of the reference count
  int DecrementRefCount()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csBlob);
#endif
    if (m_refCount > 0) --m_refCount;
    return m_refCount;
  }

  /// Invalidate instance
  void Invalidate()
  {
#if wxUSE_THREADS
    wxCriticalSectionLocker locker(gs_csBlob);
#endif
    m_isValid = false;
  }

  sqlite3_blob* m_blob;           ///< SQLite blob reference
  int           m_refCount;       ///< Reference count
  bool          m_isValid;        ///< SQLite statement reference is valid

  friend class WXDLLIMPEXP_FWD_SQLITE3 wxSQLite3Blob;
};

// ----------------------------------------------------------------------------
// inline conversion from wxString to wxLongLong
// ----------------------------------------------------------------------------

inline wxLongLong ConvertStringToLongLong(const wxString& str, wxLongLong defValue /*=0*/)
{
  size_t n = str.Length();
  size_t j = 0;
  wxLongLong value = 0;
  bool negative = false;

  if (str[j] == '-')
  {
    negative = true;
    j++;
  }

  while (j < n)
  {
    if (str[j] < '0' || str[j] > '9')
    {
      return defValue;
    }
    value *= 10;
    value += (str[j] - '0');
    j++;
  }

  return negative ? -value : value;
}

// ----------------------------------------------------------------------------
// wxSQLite3Exception: class
// ----------------------------------------------------------------------------

wxSQLite3Exception::wxSQLite3Exception(int errorCode, const wxString& errorMsg)
  : m_errorCode(errorCode)
{
  m_errorMessage = ErrorCodeAsString(errorCode) + wxS("[") +
                   wxString::Format(wxS("%d"), errorCode) + wxS("]: ") +
                   wxGetTranslation(errorMsg);
}

wxSQLite3Exception::wxSQLite3Exception(const wxSQLite3Exception&  e)
  : m_errorCode(e.m_errorCode), m_errorMessage(e.m_errorMessage)
{
}

const wxString wxSQLite3Exception::ErrorCodeAsString(int errorCode)
{
#if SQLITE_VERSION_NUMBER >= 3007015
  if (errorCode == WXSQLITE_ERROR)
  {
    return wxS("WXSQLITE_ERROR");
  }
  else
  {
    const char* errmsg = sqlite3_errstr(errorCode);
    return wxString::FromUTF8(errmsg);
  }
#else
  switch (errorCode)
  {
    case SQLITE_OK          : return wxS("SQLITE_OK");
    case SQLITE_ERROR       : return wxS("SQLITE_ERROR");
    case SQLITE_INTERNAL    : return wxS("SQLITE_INTERNAL");
    case SQLITE_PERM        : return wxS("SQLITE_PERM");
    case SQLITE_ABORT       : return wxS("SQLITE_ABORT");
    case SQLITE_BUSY        : return wxS("SQLITE_BUSY");
    case SQLITE_LOCKED      : return wxS("SQLITE_LOCKED");
    case SQLITE_NOMEM       : return wxS("SQLITE_NOMEM");
    case SQLITE_READONLY    : return wxS("SQLITE_READONLY");
    case SQLITE_INTERRUPT   : return wxS("SQLITE_INTERRUPT");
    case SQLITE_IOERR       : return wxS("SQLITE_IOERR");
    case SQLITE_CORRUPT     : return wxS("SQLITE_CORRUPT");
    case SQLITE_NOTFOUND    : return wxS("SQLITE_NOTFOUND");
    case SQLITE_FULL        : return wxS("SQLITE_FULL");
    case SQLITE_CANTOPEN    : return wxS("SQLITE_CANTOPEN");
    case SQLITE_PROTOCOL    : return wxS("SQLITE_PROTOCOL");
    case SQLITE_EMPTY       : return wxS("SQLITE_EMPTY");
    case SQLITE_SCHEMA      : return wxS("SQLITE_SCHEMA");
    case SQLITE_TOOBIG      : return wxS("SQLITE_TOOBIG");
    case SQLITE_CONSTRAINT  : return wxS("SQLITE_CONSTRAINT");
    case SQLITE_MISMATCH    : return wxS("SQLITE_MISMATCH");
    case SQLITE_MISUSE      : return wxS("SQLITE_MISUSE");
    case SQLITE_NOLFS       : return wxS("SQLITE_NOLFS");
    case SQLITE_AUTH        : return wxS("SQLITE_AUTH");
    case SQLITE_FORMAT      : return wxS("SQLITE_FORMAT");
    case SQLITE_RANGE       : return wxS("SQLITE_RANGE");
    case SQLITE_NOTADB      : return wxS("SQLITE_NOTADB");
    case SQLITE_ROW         : return wxS("SQLITE_ROW");
    case SQLITE_DONE        : return wxS("SQLITE_DONE");
    // Extended error codes
    case SQLITE_IOERR_READ       : return wxS("SQLITE_IOERR_READ");
    case SQLITE_IOERR_SHORT_READ : return wxS("SQLITE_IOERR_SHORT_READ");
    case SQLITE_IOERR_WRITE      : return wxS("SQLITE_IOERR_WRITE");
    case SQLITE_IOERR_FSYNC      : return wxS("SQLITE_IOERR_FSYNC");
    case SQLITE_IOERR_DIR_FSYNC  : return wxS("SQLITE_IOERR_DIR_FSYNC");
    case SQLITE_IOERR_TRUNCATE   : return wxS("SQLITE_IOERR_TRUNCATE");
    case SQLITE_IOERR_FSTAT      : return wxS("SQLITE_IOERR_FSTAT");
    case SQLITE_IOERR_UNLOCK     : return wxS("SQLITE_IOERR_UNLOCK");
    case SQLITE_IOERR_RDLOCK     : return wxS("SQLITE_IOERR_RDLOCK");
    case SQLITE_IOERR_DELETE     : return wxS("SQLITE_IOERR_DELETE");
#if SQLITE_VERSION_NUMBER >= 3004000
    case SQLITE_IOERR_BLOCKED    : return wxS("SQLITE_IOERR_BLOCKED");
#endif
#if SQLITE_VERSION_NUMBER >= 3005001
    case SQLITE_IOERR_NOMEM      : return wxS("SQLITE_IOERR_NOMEM");
#endif
#if SQLITE_VERSION_NUMBER >= 3006000
    case SQLITE_IOERR_ACCESS     : return wxS("SQLITE_IOERR_ACCESS");
    case SQLITE_IOERR_CHECKRESERVEDLOCK : return wxS("SQLITE_IOERR_CHECKRESERVEDLOCK");
#endif
#if SQLITE_VERSION_NUMBER >= 3006002
    case SQLITE_IOERR_LOCK       : return wxS("SQLITE_IOERR_LOCK");
#endif
#if SQLITE_VERSION_NUMBER >= 3006007
    case SQLITE_IOERR_CLOSE      : return wxS("SQLITE_IOERR_CLOSE");
    case SQLITE_IOERR_DIR_CLOSE  : return wxS("SQLITE_IOERR_DIR_CLOSE");
#endif
#if SQLITE_VERSION_NUMBER >= 3007000
    case SQLITE_IOERR_SHMOPEN      : return wxS("SQLITE_IOERR_SHMOPEN");
    case SQLITE_IOERR_SHMSIZE      : return wxS("SQLITE_IOERR_SHMSIZE");
    case SQLITE_IOERR_SHMLOCK      : return wxS("SQLITE_IOERR_SHMLOCK");
    case SQLITE_LOCKED_SHAREDCACHE : return wxS("SQLITE_LOCKED_SHAREDCACHE");
    case SQLITE_BUSY_RECOVERY      : return wxS("SQLITE_BUSY_RECOVERY");
    case SQLITE_CANTOPEN_NOTEMPDIR : return wxS("SQLITE_CANTOPEN_NOTEMPDIR");
 #endif
#if SQLITE_VERSION_NUMBER >= 3007007
    case SQLITE_CORRUPT_VTAB       : return wxS("SQLITE_CORRUPT_VTAB");
    case SQLITE_READONLY_RECOVERY  : return wxS("SQLITE_READONLY_RECOVERY");
    case SQLITE_READONLY_CANTLOCK  : return wxS("SQLITE_READONLY_CANTLOCK");
#endif

    case WXSQLITE_ERROR     : return wxS("WXSQLITE_ERROR");
    default                 : return wxS("UNKNOWN_ERROR");
  }
#endif
}

wxSQLite3Exception::~wxSQLite3Exception()
{
}

// ----------------------------------------------------------------------------
// wxSQLite3StatementBuffer: class providing a statement buffer
//                           for use with the SQLite3 vmprintf function
// ----------------------------------------------------------------------------

wxSQLite3StatementBuffer::wxSQLite3StatementBuffer()
{
  m_buffer = 0;
}

wxSQLite3StatementBuffer::~wxSQLite3StatementBuffer()
{
  Clear();
}

void wxSQLite3StatementBuffer::Clear()
{
  if (m_buffer)
  {
    sqlite3_free(m_buffer);
    m_buffer = 0;
  }
}

const char* wxSQLite3StatementBuffer::Format(const char* format, ...)
{
  Clear();
  va_list va;
  va_start(va, format);
  m_buffer = sqlite3_vmprintf(format, va);
  va_end(va);
  return m_buffer;
}

const char* wxSQLite3StatementBuffer::FormatV(const char* format, va_list va)
{
  Clear();
  m_buffer = sqlite3_vmprintf(format, va);
  return m_buffer;
}

// ----------------------------------------------------------------------------
// wxSQLite3ResultSet: class providing access to the result set of a query
// ----------------------------------------------------------------------------

wxSQLite3ResultSet::wxSQLite3ResultSet()
{
  m_db = NULL;
  m_stmt = NULL;
  m_eof = true;
  m_first = true;
  m_cols = 0;
}

wxSQLite3ResultSet::wxSQLite3ResultSet(const wxSQLite3ResultSet& resultSet)
{
  m_db = resultSet.m_db;
  if (m_db != NULL)
  {
    m_db->IncrementRefCount();
  }
  m_stmt = resultSet.m_stmt;
  if (m_stmt != NULL)
  {
    m_stmt->IncrementRefCount();
  }
  m_eof = resultSet.m_eof;
  m_first = resultSet.m_first;
  m_cols = resultSet.m_cols;
}

wxSQLite3ResultSet::wxSQLite3ResultSet(wxSQLite3DatabaseReference* db,
                                       wxSQLite3StatementReference* stmt,
                                       bool eof,
                                       bool first)
{
  m_db = db;
  if (m_db != NULL)
  {
    m_db->IncrementRefCount();
  }
  m_stmt = stmt;
  if (m_stmt != NULL)
  {
    m_stmt->IncrementRefCount();
  }
  CheckStmt();
  m_eof = eof;
  m_first = first;
  m_cols = (m_stmt != NULL) ? sqlite3_column_count(m_stmt->m_stmt) : 0;
}

wxSQLite3ResultSet::~wxSQLite3ResultSet()
{
  if (m_stmt != NULL && m_stmt->DecrementRefCount() == 0)
  {
    if (m_stmt->m_isValid)
    {
      try
      {
        Finalize(m_db, m_stmt);
      }
      catch (...)
      {
      }
    }
    delete m_stmt;
  }
  if (m_db != NULL && m_db->DecrementRefCount() == 0)
  {
    if (m_db->m_isValid)
    {
      sqlite3_close(m_db->m_db);
    }
    delete m_db;
  }
}

wxSQLite3ResultSet& wxSQLite3ResultSet::operator=(const wxSQLite3ResultSet& resultSet)
{
  if (this != &resultSet)
  {
    wxSQLite3DatabaseReference* dbPrev = m_db;
    wxSQLite3StatementReference* stmtPrev = m_stmt;
    m_db = resultSet.m_db;
    if (m_db != NULL)
    {
      m_db->IncrementRefCount();
    }
    m_stmt = resultSet.m_stmt;
    if (m_stmt != NULL)
    {
      m_stmt->IncrementRefCount();
    }
    m_eof = resultSet.m_eof;
    m_first = resultSet.m_first;
    m_cols = resultSet.m_cols;
    if (stmtPrev != NULL && stmtPrev->DecrementRefCount() == 0)
    {
      Finalize(dbPrev, stmtPrev);
      delete stmtPrev;
    }
    if (dbPrev != NULL && dbPrev->DecrementRefCount() == 0)
    {
      delete dbPrev;
    }
  }
  return *this;
}

int wxSQLite3ResultSet::GetColumnCount() const
{
  CheckStmt();
  return m_cols;
}

wxString wxSQLite3ResultSet::GetAsString(int columnIndex) const
{
  CheckStmt();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  const char* localValue = (const char*) sqlite3_column_text(m_stmt->m_stmt, columnIndex);
  return wxString::FromUTF8(localValue);
}

wxString wxSQLite3ResultSet::GetAsString(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  const char* localValue = (const char*) sqlite3_column_text(m_stmt->m_stmt, columnIndex);
  return wxString::FromUTF8(localValue);
}

int wxSQLite3ResultSet::GetInt(int columnIndex, int nullValue /* = 0 */) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return nullValue;
  }
  else
  {
    return sqlite3_column_int(m_stmt->m_stmt, columnIndex);
  }
}


int wxSQLite3ResultSet::GetInt(const wxString& columnName, int nullValue /* = 0 */) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetInt(columnIndex, nullValue);
}

wxLongLong wxSQLite3ResultSet::GetInt64(int columnIndex, wxLongLong nullValue /* = 0 */) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return nullValue;
  }
  else
  {
    return wxLongLong(sqlite3_column_int64(m_stmt->m_stmt, columnIndex));
  }
}

wxLongLong wxSQLite3ResultSet::GetInt64(const wxString& columnName, wxLongLong nullValue /* = 0 */) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetInt64(columnIndex, nullValue);
}

double wxSQLite3ResultSet::GetDouble(int columnIndex, double nullValue /* = 0.0 */) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return nullValue;
  }
  else
  {
    return sqlite3_column_double(m_stmt->m_stmt, columnIndex);
  }
}

double wxSQLite3ResultSet::GetDouble(const wxString& columnName, double nullValue /* = 0.0 */) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetDouble(columnIndex, nullValue);
}

wxString wxSQLite3ResultSet::GetString(int columnIndex, const wxString& nullValue /* = wxEmptyString */) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return nullValue;
  }
  else
  {
    const char* localValue = (const char*) sqlite3_column_text(m_stmt->m_stmt, columnIndex);
    return wxString::FromUTF8(localValue);
  }
}

wxString wxSQLite3ResultSet::GetString(const wxString& columnName, const wxString& nullValue /* = wxEmptyString */) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetString(columnIndex, nullValue);
}

const unsigned char* wxSQLite3ResultSet::GetBlob(int columnIndex, int& len) const
{
  CheckStmt();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  len = sqlite3_column_bytes(m_stmt->m_stmt, columnIndex);
  return (const unsigned char*) sqlite3_column_blob(m_stmt->m_stmt, columnIndex);
}

const unsigned char* wxSQLite3ResultSet::GetBlob(const wxString& columnName, int& len) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetBlob(columnIndex, len);
}

wxMemoryBuffer& wxSQLite3ResultSet::GetBlob(int columnIndex, wxMemoryBuffer& buffer) const
{
  CheckStmt();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  int len = sqlite3_column_bytes(m_stmt->m_stmt, columnIndex);
  const void* blob = sqlite3_column_blob(m_stmt->m_stmt, columnIndex);
  buffer.AppendData((void*) blob, (size_t) len);
  return buffer;
}

wxMemoryBuffer& wxSQLite3ResultSet::GetBlob(const wxString& columnName, wxMemoryBuffer& buffer) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetBlob(columnIndex, buffer);
}

wxDateTime wxSQLite3ResultSet::GetDate(int columnIndex) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return wxInvalidDateTime;
  }
  else
  {
    wxDateTime date;
    const wxChar* result = date.ParseDate(GetString(columnIndex));
    if (result != NULL)
    {
      return date;
    }
    else
    {
      return wxInvalidDateTime;
    }
  }
}

wxDateTime wxSQLite3ResultSet::GetDate(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetDate(columnIndex);
}


wxDateTime wxSQLite3ResultSet::GetTime(int columnIndex) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return wxInvalidDateTime;
  }
  else
  {
    wxDateTime date;
    const wxChar* result = date.ParseTime(GetString(columnIndex));
    if (result != NULL)
    {
      return date;
    }
    else
    {
      return wxInvalidDateTime;
    }
  }
}

wxDateTime wxSQLite3ResultSet::GetTime(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetTime(columnIndex);
}

wxDateTime wxSQLite3ResultSet::GetDateTime(int columnIndex) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return wxInvalidDateTime;
  }
  else
  {
    wxDateTime date;
    const wxChar* result = date.ParseDateTime(GetString(columnIndex));
    if (result != NULL)
    {
      date.SetMillisecond(0);
      return date;
    }
    else
    {
      return wxInvalidDateTime;
    }
  }
}

wxDateTime wxSQLite3ResultSet::GetDateTime(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetDateTime(columnIndex);
}

wxDateTime wxSQLite3ResultSet::GetTimestamp(int columnIndex) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return wxInvalidDateTime;
  }
  else
  {
    wxDateTime date;
    const wxChar* result = date.ParseDateTime(GetString(columnIndex));
    if (result != NULL)
    {
      return date;
    }
    else
    {
      return wxInvalidDateTime;
    }
  }
}

wxDateTime wxSQLite3ResultSet::GetTimestamp(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetTimestamp(columnIndex);
}

wxDateTime wxSQLite3ResultSet::GetNumericDateTime(int columnIndex) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return wxInvalidDateTime;
  }
  else
  {
    wxLongLong value = GetInt64(columnIndex);
    return wxDateTime(value);
  }
}

wxDateTime wxSQLite3ResultSet::GetNumericDateTime(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetNumericDateTime(columnIndex);
}

wxDateTime wxSQLite3ResultSet::GetUnixDateTime(int columnIndex) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return wxInvalidDateTime;
  }
  else
  {
    wxLongLong value = GetInt64(columnIndex);
    return wxDateTime((time_t) value.GetValue());
  }
}

wxDateTime wxSQLite3ResultSet::GetUnixDateTime(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetUnixDateTime(columnIndex);
}

wxDateTime wxSQLite3ResultSet::GetJulianDayNumber(int columnIndex) const
{
  if (GetColumnType(columnIndex) == SQLITE_NULL)
  {
    return wxInvalidDateTime;
  }
  else
  {
    double value = GetDouble(columnIndex);
    return wxDateTime(value);
  }
}

wxDateTime wxSQLite3ResultSet::GetJulianDayNumber(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetJulianDayNumber(columnIndex);
}

bool wxSQLite3ResultSet::GetBool(int columnIndex) const
{
  return GetInt(columnIndex) != 0;
}

wxDateTime wxSQLite3ResultSet::GetAutomaticDateTime(int columnIndex, bool milliSeconds) const
{
  wxDateTime result;
  int columnType = GetColumnType(columnIndex);
  switch (columnType)
  {
    case SQLITE3_TEXT:
      result = GetDateTime(columnIndex);
      break;
    case SQLITE_INTEGER:
      if (milliSeconds)
      {
        wxLongLong value = GetInt64(columnIndex);
        result = wxDateTime(value);
      }
      else
      {
        time_t value = GetInt64(columnIndex).GetValue();
        result = wxDateTime(value);
      }
      break;
    case SQLITE_FLOAT:
      result = GetJulianDayNumber(columnIndex);
      break;
    case SQLITE_NULL:
    default:
      result = wxInvalidDateTime;
      break;
  }
  return result;
}

wxDateTime wxSQLite3ResultSet::GetAutomaticDateTime(const wxString& columnName, bool milliSeconds) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetAutomaticDateTime(columnIndex, milliSeconds);
}

bool wxSQLite3ResultSet::GetBool(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetBool(columnIndex);
}

bool wxSQLite3ResultSet::IsNull(int columnIndex) const
{
  return (GetColumnType(columnIndex) == SQLITE_NULL);
}

bool wxSQLite3ResultSet::IsNull(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return (GetColumnType(columnIndex) == SQLITE_NULL);
}

int wxSQLite3ResultSet::FindColumnIndex(const wxString& columnName) const
{
  CheckStmt();

  wxCharBuffer strColumnName = columnName.ToUTF8();
  const char* localColumnName = strColumnName;

  if (columnName.Len() > 0)
  {
    for (int columnIndex = 0; columnIndex < m_cols; columnIndex++)
    {
      const char* temp = sqlite3_column_name(m_stmt->m_stmt, columnIndex);

      if (strcmp(localColumnName, temp) == 0)
      {
        return columnIndex;
      }
    }
  }

  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
}

wxString wxSQLite3ResultSet::GetColumnName(int columnIndex) const
{
  CheckStmt();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  const char* localValue = sqlite3_column_name(m_stmt->m_stmt, columnIndex);
  return wxString::FromUTF8(localValue);
}

wxString wxSQLite3ResultSet::GetDeclaredColumnType(int columnIndex) const
{
  CheckStmt();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  const char* localValue = sqlite3_column_decltype(m_stmt->m_stmt, columnIndex);
  return wxString::FromUTF8(localValue);
}

int wxSQLite3ResultSet::GetColumnType(int columnIndex) const
{
  CheckStmt();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  return sqlite3_column_type(m_stmt->m_stmt, columnIndex);
}

bool wxSQLite3ResultSet::Eof() const
{
  CheckStmt();
  return m_eof;
}

bool wxSQLite3ResultSet::CursorMoved() const
{
  CheckStmt();
  return !m_first;
}

bool wxSQLite3ResultSet::NextRow()
{
  CheckStmt();

  int rc;
  if (m_first)
  {
    m_first = false;
    rc = (m_eof) ? SQLITE_DONE : SQLITE_ROW;
  }
  else
  {
    rc = sqlite3_step(m_stmt->m_stmt);
  }

  if (rc == SQLITE_DONE) // no more rows
  {
    m_eof = true;
    return false;
  }
  else if (rc == SQLITE_ROW) // more rows
  {
    return true;
  }
  else
  {
    rc = sqlite3_finalize(m_stmt->m_stmt);
    m_stmt->Invalidate();
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

void wxSQLite3ResultSet::Finalize()
{
  Finalize(m_db, m_stmt);
  if (m_stmt != NULL && m_stmt->DecrementRefCount() == 0)
  {
    delete m_stmt;
  }
  m_stmt = NULL;
  if (m_db != NULL && m_db->DecrementRefCount() == 0)
  {
    if (m_db->m_isValid)
    {
      sqlite3_close(m_db->m_db);
    }
    delete m_db;
  }
  m_db = NULL;
}

void wxSQLite3ResultSet::Finalize(wxSQLite3DatabaseReference* db,wxSQLite3StatementReference* stmt)
{
  if (stmt != NULL && stmt->m_isValid)
  {
    int rc = sqlite3_finalize(stmt->m_stmt);
    stmt->Invalidate();
    if (rc != SQLITE_OK)
    {
      if (db != NULL && db->m_isValid)
      {
        const char* localError = sqlite3_errmsg(db->m_db);
        throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
      }
      else
      {
        throw wxSQLite3Exception(rc, wxERRMSG_FINALIZE_FAILED);
      }
    }
  }
}

wxString wxSQLite3ResultSet::GetSQL() const
{
  wxString sqlString = wxEmptyString;
#if SQLITE_VERSION_NUMBER >= 3005003
  CheckStmt();
  const char* sqlLocal = sqlite3_sql(m_stmt->m_stmt);
  if (sqlLocal != NULL) sqlString = wxString::FromUTF8(sqlLocal);
#endif
  return sqlString;
}

wxString wxSQLite3ResultSet::GetExpandedSQL() const
{
  wxString sqlString = wxEmptyString;
#if SQLITE_VERSION_NUMBER >= 3014000
  CheckStmt();
  char* sqlLocal = sqlite3_expanded_sql(m_stmt->m_stmt);
  if (sqlLocal != NULL)
  {
    sqlString = wxString::FromUTF8(sqlLocal);
    sqlite3_free(sqlLocal);
  }
#endif
  return sqlString;
}

bool wxSQLite3ResultSet::IsOk() const
{
  return (m_db != NULL) && (m_db->m_isValid) && (m_stmt != NULL) && (m_stmt->m_isValid);
}

void wxSQLite3ResultSet::CheckStmt() const
{
  if (m_stmt == NULL || m_stmt->m_stmt == NULL || !m_stmt->m_isValid)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOSTMT);
  }
}

wxString wxSQLite3ResultSet::GetDatabaseName(int columnIndex) const
{
#if SQLITE_ENABLE_COLUMN_METADATA
  CheckStmt();
  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  const char* localValue = sqlite3_column_database_name(m_stmt->m_stmt, columnIndex);
  if (localValue != NULL)
    return wxString::FromUTF8(localValue);
  else
    return wxEmptyString;
#else
  wxUnusedVar(columnIndex);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOMETADATA);
#endif
}

wxString wxSQLite3ResultSet::GetTableName(int columnIndex) const
{
#if SQLITE_ENABLE_COLUMN_METADATA
  CheckStmt();
  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  const char* localValue = sqlite3_column_table_name(m_stmt->m_stmt, columnIndex);
  if (localValue != NULL)
    return wxString::FromUTF8(localValue);
  else
    return wxEmptyString;
#else
  wxUnusedVar(columnIndex);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOMETADATA);
#endif
}

wxString wxSQLite3ResultSet::GetOriginName(int columnIndex) const
{
#if SQLITE_ENABLE_COLUMN_METADATA
  CheckStmt();
  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  const char* localValue = sqlite3_column_origin_name(m_stmt->m_stmt, columnIndex);
  if (localValue != NULL)
    return wxString::FromUTF8(localValue);
  else
    return wxEmptyString;
#else
  wxUnusedVar(columnIndex);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOMETADATA);
#endif
}

// ----------------------------------------------------------------------------
// wxSQLite3Table: class holding the complete result set of a query
// ----------------------------------------------------------------------------

wxSQLite3Table::wxSQLite3Table()
{
  m_results = 0;
  m_rows = 0;
  m_cols = 0;
  m_currentRow = 0;
}

wxSQLite3Table::wxSQLite3Table(const wxSQLite3Table& table)
{
  m_results = table.m_results;
  // Only one object can own the results
  const_cast<wxSQLite3Table&>(table).m_results = 0;
  m_rows = table.m_rows;
  m_cols = table.m_cols;
  m_currentRow = table.m_currentRow;
}

wxSQLite3Table::wxSQLite3Table(char** results, int rows, int cols)
{
  m_results = results;
  m_rows = rows;
  m_cols = cols;
  m_currentRow = 0;
}

wxSQLite3Table::~wxSQLite3Table()
{
  try
  {
    Finalize();
  }
  catch (...)
  {
  }
}

wxSQLite3Table& wxSQLite3Table::operator=(const wxSQLite3Table& table)
{
  if (this != &table)
  {
    try
    {
      Finalize();
    }
    catch (...)
    {
    }
    m_results = table.m_results;
    // Only one object can own the results
    const_cast<wxSQLite3Table&>(table).m_results = 0;
    m_rows = table.m_rows;
    m_cols = table.m_cols;
    m_currentRow = table.m_currentRow;
  }
  return *this;
}

void wxSQLite3Table::Finalize()
{
  if (m_results)
  {
    sqlite3_free_table(m_results);
    m_results = 0;
  }
}

int wxSQLite3Table::GetColumnCount() const
{
  CheckResults();
  return m_cols;
}

int wxSQLite3Table::GetRowCount() const
{
  CheckResults();
  return m_rows;
}

int wxSQLite3Table::FindColumnIndex(const wxString& columnName) const
{
  CheckResults();

  wxCharBuffer strColumnName = columnName.ToUTF8();
  const char* localColumnName = strColumnName;

  if (columnName.Len() > 0)
  {
    for (int columnIndex = 0; columnIndex < m_cols; columnIndex++)
    {
      if (strcmp(localColumnName, m_results[columnIndex]) == 0)
      {
        return columnIndex;
      }
    }
  }

  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_NAME);
}

wxString wxSQLite3Table::GetAsString(int columnIndex) const
{
  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  int nIndex = (m_currentRow*m_cols) + m_cols + columnIndex;
  const char* localValue = m_results[nIndex];
  return wxString::FromUTF8(localValue);
}

wxString wxSQLite3Table::GetAsString(const wxString& columnName) const
{
  int index = FindColumnIndex(columnName);
  return GetAsString(index);
}

int wxSQLite3Table::GetInt(int columnIndex, int nullValue /* = 0 */) const
{
  if (IsNull(columnIndex))
  {
    return nullValue;
  }
  else
  {
    long value = nullValue;
    GetAsString(columnIndex).ToLong(&value);
    return (int) value;
  }
}

int wxSQLite3Table::GetInt(const wxString& columnName, int nullValue /* = 0 */) const
{
  if (IsNull(columnName))
  {
    return nullValue;
  }
  else
  {
    long value = nullValue;
    GetAsString(columnName).ToLong(&value);
    return (int) value;
  }
}

wxLongLong wxSQLite3Table::GetInt64(int columnIndex, wxLongLong nullValue /* = 0 */) const
{
  if (IsNull(columnIndex))
  {
    return nullValue;
  }
  else
  {
    return ConvertStringToLongLong(GetAsString(columnIndex), nullValue);
  }
}

wxLongLong wxSQLite3Table::GetInt64(const wxString& columnName, wxLongLong nullValue /* = 0 */) const
{
  if (IsNull(columnName))
  {
    return nullValue;
  }
  else
  {
    return ConvertStringToLongLong(GetAsString(columnName), nullValue);
  }
}

// Since SQLite uses internally a locale independent string representation
// of double values, we need to provide our own conversion procedure using
// always a point as the decimal separator.
// The following code duplicates a SQLite utility function with minor modifications.

static double wxSQLite3AtoF(const char *z)
{
  int sign = 1;
  long double v1 = 0.0;
  int nSignificant = 0;
  while (isspace(*(unsigned char*)z))
  {
    ++z;
  }
  if (*z == '-')
  {
    sign = -1;
    ++z;
  }
  else if (*z == '+')
  {
    ++z;
  }
  while (*z == '0')
  {
    ++z;
  }
  while (isdigit(*(unsigned char*)z))
  {
    v1 = v1*10.0 + (*z - '0');
    ++z;
    ++nSignificant;
  }
  if (*z == '.')
  {
    long double divisor = 1.0;
    ++z;
    if (nSignificant == 0)
    {
      while (*z == '0')
      {
        divisor *= 10.0;
        ++z;
      }
    }
    while (isdigit(*(unsigned char*)z))
    {
      if (nSignificant < 18)
      {
        v1 = v1*10.0 + (*z - '0');
        divisor *= 10.0;
        ++nSignificant;
      }
      ++z;
    }
    v1 /= divisor;
  }
  if (*z=='e' || *z=='E')
  {
    int esign = 1;
    int eval = 0;
    long double scale = 1.0;
    ++z;
    if (*z == '-')
    {
      esign = -1;
      ++z;
    }
    else if (*z == '+')
    {
      ++z;
    }
    while (isdigit(*(unsigned char*)z))
    {
      eval = eval*10 + *z - '0';
      ++z;
    }
    while (eval >= 64) { scale *= 1.0e+64; eval -= 64; }
    while (eval >= 16) { scale *= 1.0e+16; eval -= 16; }
    while (eval >=  4) { scale *= 1.0e+4;  eval -= 4; }
    while (eval >=  1) { scale *= 1.0e+1;  eval -= 1; }
    if (esign < 0)
    {
      v1 /= scale;
    }
    else
    {
      v1 *= scale;
    }
  }
  return (double) ((sign < 0) ? -v1 : v1);
}

double wxSQLite3Table::GetDouble(int columnIndex, double nullValue /* = 0.0 */) const
{
  if (IsNull(columnIndex))
  {
    return nullValue;
  }
  else
  {
    if (columnIndex < 0 || columnIndex > m_cols-1)
    {
      throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
    }
    int nIndex = (m_currentRow*m_cols) + m_cols + columnIndex;
    return wxSQLite3AtoF(m_results[nIndex]);
  }
}

double wxSQLite3Table::GetDouble(const wxString& columnName, double nullValue /* = 0.0 */) const
{
  int index = FindColumnIndex(columnName);
  return GetDouble(index, nullValue);
}

wxString wxSQLite3Table::GetString(int columnIndex, const wxString& nullValue /* = wxEmptyString */) const
{
  if (IsNull(columnIndex))
  {
    return nullValue;
  }
  else
  {
    return GetAsString(columnIndex);
  }
}

wxString wxSQLite3Table::GetString(const wxString& columnName, const wxString& nullValue /* = wxEmptyString */) const
{
  if (IsNull(columnName))
  {
    return nullValue;
  }
  else
  {
    return GetAsString(columnName);
  }
}

wxDateTime wxSQLite3Table::GetDate(int columnIndex) const
{
  wxDateTime date;
  const wxChar* result = date.ParseDate(GetString(columnIndex));
  if (result != NULL)
  {
    return date;
  }
  else
  {
    return wxInvalidDateTime;
  }
}

wxDateTime wxSQLite3Table::GetDate(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetDate(columnIndex);
}

wxDateTime wxSQLite3Table::GetTime(int columnIndex) const
{
  wxDateTime date;
  const wxChar* result = date.ParseTime(GetString(columnIndex));
  if (result != NULL)
  {
    return date;
  }
  else
  {
    return wxInvalidDateTime;
  }
}

wxDateTime wxSQLite3Table::GetTime(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetTime(columnIndex);
}

wxDateTime wxSQLite3Table::GetDateTime(int columnIndex) const
{
  wxDateTime date;
  const wxChar* result = date.ParseDateTime(GetString(columnIndex));
  if (result != NULL)
  {
    return date;
  }
  else
  {
    return wxInvalidDateTime;
  }
}

wxDateTime wxSQLite3Table::GetDateTime(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetDateTime(columnIndex);
}

bool wxSQLite3Table::GetBool(int columnIndex) const
{
  return GetInt(columnIndex) != 0;
}

bool wxSQLite3Table::GetBool(const wxString& columnName) const
{
  int columnIndex = FindColumnIndex(columnName);
  return GetBool(columnIndex);
}

bool wxSQLite3Table::IsNull(int columnIndex) const
{
  CheckResults();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  int index = (m_currentRow*m_cols) + m_cols + columnIndex;
  const char* localValue = m_results[index];
  return (localValue == 0);
}

bool wxSQLite3Table::IsNull(const wxString& columnName) const
{
  int index = FindColumnIndex(columnName);
  return IsNull(index);
}

wxString wxSQLite3Table::GetColumnName(int columnIndex) const
{
  CheckResults();

  if (columnIndex < 0 || columnIndex > m_cols-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_INDEX);
  }

  const char* localValue = m_results[columnIndex];
  return wxString::FromUTF8(localValue);
}

void wxSQLite3Table::SetRow(int row)
{
  CheckResults();

  if (row < 0 || row > m_rows-1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_ROW);
  }

  m_currentRow = row;
}

bool wxSQLite3Table::IsOk() const
{
  return (m_results != 0);
}

void wxSQLite3Table::CheckResults() const
{
  if (m_results == 0)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NORESULT);
  }
}

// ----------------------------------------------------------------------------
// wxSQLite3Statement: class holding a prepared statement
// ----------------------------------------------------------------------------

wxSQLite3Statement::wxSQLite3Statement()
{
  m_db = 0;
  m_stmt = 0;
}

wxSQLite3Statement::wxSQLite3Statement(const wxSQLite3Statement& statement)
{
  m_db = statement.m_db;
  if (m_db != NULL)
  {
    m_db->IncrementRefCount();
  }
  m_stmt = statement.m_stmt;
  if (m_stmt != NULL)
  {
    m_stmt->IncrementRefCount();
  }
}

wxSQLite3Statement::wxSQLite3Statement(wxSQLite3DatabaseReference* db, wxSQLite3StatementReference* stmt)
{
  m_db = db;
  if (m_db != NULL)
  {
    m_db->IncrementRefCount();
  }
  m_stmt = stmt;
  if (m_stmt != NULL)
  {
    m_stmt->IncrementRefCount();
  }
}

wxSQLite3Statement::~wxSQLite3Statement()
{
  if (m_stmt != NULL && m_stmt->DecrementRefCount() == 0)
  {
    if (m_stmt->m_isValid)
    {
      try
      {
        Finalize(m_db, m_stmt);
      }
      catch (...)
      {
      }
    }
    delete m_stmt;
  }
  if (m_db != NULL && m_db->DecrementRefCount() == 0)
  {
    if (m_db->m_isValid)
    {
      sqlite3_close(m_db->m_db);
    }
    delete m_db;
  }
}

wxSQLite3Statement& wxSQLite3Statement::operator=(const wxSQLite3Statement& statement)
{
  if (this != &statement)
  {
    wxSQLite3DatabaseReference* dbPrev = m_db;
    wxSQLite3StatementReference* stmtPrev = m_stmt;
    m_db = statement.m_db;
    if (m_db != NULL)
    {
      m_db->IncrementRefCount();
    }
    m_stmt = statement.m_stmt;
    if (m_stmt != NULL)
    {
      m_stmt->IncrementRefCount();
    }
    if (stmtPrev != NULL && stmtPrev->DecrementRefCount() == 0)
    {
      Finalize(dbPrev, stmtPrev);
      delete stmtPrev;
    }
    if (dbPrev != NULL && dbPrev->DecrementRefCount() == 0)
    {
      delete dbPrev;
    }
  }
  return *this;
}

int wxSQLite3Statement::ExecuteUpdate()
{
  CheckDatabase();
  CheckStmt();

  const char* localError=0;

  int rc = sqlite3_step(m_stmt->m_stmt);

  if (rc == SQLITE_DONE)
  {
    int rowsChanged = sqlite3_changes(m_db->m_db);

    rc = sqlite3_reset(m_stmt->m_stmt);

    if (rc != SQLITE_OK)
    {
      localError = sqlite3_errmsg(m_db->m_db);
      throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }

    return rowsChanged;
  }
  else
  {
    rc = sqlite3_reset(m_stmt->m_stmt);
    localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

wxSQLite3ResultSet wxSQLite3Statement::ExecuteQuery()
{
  CheckDatabase();
  CheckStmt();

  int rc = sqlite3_step(m_stmt->m_stmt);

  if (rc == SQLITE_DONE)  // no more rows
  {
    return wxSQLite3ResultSet(m_db, m_stmt, true/*eof*/, true/*first*/);
  }
  else if (rc == SQLITE_ROW)  // one or more rows
  {
    return wxSQLite3ResultSet(m_db, m_stmt, false/*eof*/, true/*first*/);
  }
  else
  {
    rc = sqlite3_reset(m_stmt->m_stmt);
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

int wxSQLite3Statement::ExecuteScalar()
{
  wxSQLite3ResultSet resultSet = ExecuteQuery();

  if (resultSet.Eof() || resultSet.GetColumnCount() < 1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_QUERY);
  }

  long value = 0;
  resultSet.GetAsString(0).ToLong(&value);
  return (int) value;
}

int wxSQLite3Statement::GetParamCount() const
{
  CheckStmt();
  return sqlite3_bind_parameter_count(m_stmt->m_stmt);
}

int wxSQLite3Statement::GetParamIndex(const wxString& paramName) const
{
  CheckStmt();

  wxCharBuffer strParamName = paramName.ToUTF8();
  const char* localParamName = strParamName;

  return sqlite3_bind_parameter_index(m_stmt->m_stmt, localParamName);
}

wxString wxSQLite3Statement::GetParamName(int paramIndex) const
{
  CheckStmt();
  const char* localParamName = sqlite3_bind_parameter_name(m_stmt->m_stmt, paramIndex);
  return wxString::FromUTF8(localParamName);
}

void wxSQLite3Statement::Bind(int paramIndex, const wxString& stringValue)
{
  CheckStmt();

  wxCharBuffer strStringValue = stringValue.ToUTF8();
  const char* localStringValue = strStringValue;

  int rc = sqlite3_bind_text(m_stmt->m_stmt, paramIndex, localStringValue, -1, SQLITE_TRANSIENT);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_STR);
  }
}

void wxSQLite3Statement::Bind(int paramIndex, int intValue)
{
  CheckStmt();
  int rc = sqlite3_bind_int(m_stmt->m_stmt, paramIndex, intValue);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_INT);
  }
}

void wxSQLite3Statement::Bind(int paramIndex, wxLongLong int64Value)
{
  CheckStmt();
  int rc = sqlite3_bind_int64(m_stmt->m_stmt, paramIndex, int64Value.GetValue());

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_INT64);
  }
}

void wxSQLite3Statement::Bind(int paramIndex, double doubleValue)
{
  CheckStmt();
  int rc = sqlite3_bind_double(m_stmt->m_stmt, paramIndex, doubleValue);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_DBL);
  }
}

void wxSQLite3Statement::Bind(int paramIndex, const char* charValue)
{
  CheckStmt();
  int rc = sqlite3_bind_text(m_stmt->m_stmt, paramIndex, charValue, -1, SQLITE_TRANSIENT);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_STR);
  }
}

void wxSQLite3Statement::Bind(int paramIndex, const unsigned char* blobValue, int blobLen)
{
  CheckStmt();
  int rc = sqlite3_bind_blob(m_stmt->m_stmt, paramIndex,
                             (const void*)blobValue, blobLen, SQLITE_TRANSIENT);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_BLOB);
  }
}

void wxSQLite3Statement::Bind(int paramIndex, const wxMemoryBuffer& blobValue)
{
  CheckStmt();
  int blobLen = (int) blobValue.GetDataLen();
  int rc = sqlite3_bind_blob(m_stmt->m_stmt, paramIndex,
                             (const void*)blobValue.GetData(), blobLen, SQLITE_TRANSIENT);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_BLOB);
  }
}

void wxSQLite3Statement::Bind(int paramIndex, void* pointer, const wxString& pointerType, void(*DeletePointer)(void*))
{
#if SQLITE_VERSION_NUMBER >= 3020000
  CheckStmt();

  const char* localPointerType = m_stmt->MakePointerTypeCopy(pointerType);
  int rc = sqlite3_bind_pointer(m_stmt->m_stmt, paramIndex, pointer, localPointerType, DeletePointer);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_POINTER);
  }
#else
  wxUnusedVar(paramIndex);
  wxUnusedVar(pointer);
  wxUnusedVar(pointerType);
  wxUnusedVar(DeletePointer);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOPOINTER);
#endif
}

void wxSQLite3Statement::BindDate(int paramIndex, const wxDateTime& date)
{
  if (date.IsValid())
  {
    Bind(paramIndex,date.FormatISODate());
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_BIND_DATETIME);
  }
}

void wxSQLite3Statement::BindTime(int paramIndex, const wxDateTime& time)
{
  if (time.IsValid())
  {
    Bind(paramIndex,time.FormatISOTime());
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_BIND_DATETIME);
  }
}

void wxSQLite3Statement::BindDateTime(int paramIndex, const wxDateTime& datetime)
{
  if (datetime.IsValid())
  {
    Bind(paramIndex,datetime.Format(wxS("%Y-%m-%d %H:%M:%S")));
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_BIND_DATETIME);
  }
}

void wxSQLite3Statement::BindTimestamp(int paramIndex, const wxDateTime& timestamp)
{
  if (timestamp.IsValid())
  {
    Bind(paramIndex,timestamp.Format(wxS("%Y-%m-%d %H:%M:%S.%l")));
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_BIND_DATETIME);
  }
}

void wxSQLite3Statement::BindNumericDateTime(int paramIndex, const wxDateTime& datetime)
{
  if (datetime.IsValid())
  {
    Bind(paramIndex, datetime.GetValue());
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_BIND_DATETIME);
  }
}

void wxSQLite3Statement::BindUnixDateTime(int paramIndex, const wxDateTime& datetime)
{
  if (datetime.IsValid())
  {
    wxLongLong ticks = datetime.GetTicks();
    Bind(paramIndex, ticks);
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_BIND_DATETIME);
  }
}

void wxSQLite3Statement::BindJulianDayNumber(int paramIndex, const wxDateTime& datetime)
{
  if (datetime.IsValid())
  {
    Bind(paramIndex, datetime.GetJulianDayNumber());
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_BIND_DATETIME);
  }
}

void wxSQLite3Statement::BindBool(int paramIndex, bool value)
{
  Bind(paramIndex, value ? 1 : 0);
}

void wxSQLite3Statement::BindNull(int paramIndex)
{
  CheckStmt();
  int rc = sqlite3_bind_null(m_stmt->m_stmt, paramIndex);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_NULL);
  }
}

void wxSQLite3Statement::BindZeroBlob(int paramIndex, int blobSize)
{
#if SQLITE_VERSION_NUMBER >= 3004000
  CheckStmt();
  int rc = sqlite3_bind_zeroblob(m_stmt->m_stmt, paramIndex, blobSize);
  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_ZEROBLOB);
  }
#else
  wxUnusedVar(paramIndex);
  wxUnusedVar(blobSize);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOINCBLOB);
#endif
}

void wxSQLite3Statement::ClearBindings()
{
  CheckStmt();
#if 0 // missing in SQLite DLL
  int rc = sqlite3_clear_bindings(m_stmt->m_stmt);

  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_BIND_CLEAR);
  }
#else
  for (int paramIndex = 1; paramIndex <= GetParamCount(); paramIndex++)
  {
    BindNull(paramIndex);
  }
#endif
}

wxString wxSQLite3Statement::GetSQL() const
{
  wxString sqlString = wxEmptyString;
#if SQLITE_VERSION_NUMBER >= 3005003
  CheckStmt();
  const char* sqlLocal = sqlite3_sql(m_stmt->m_stmt);
  if (sqlLocal != NULL) sqlString = wxString::FromUTF8(sqlLocal);
#endif
  return sqlString;
}

wxString wxSQLite3Statement::GetExpandedSQL() const
{
  wxString sqlString = wxEmptyString;
#if SQLITE_VERSION_NUMBER >= 3014000
  CheckStmt();
  char* sqlLocal = sqlite3_expanded_sql(m_stmt->m_stmt);
  if (sqlLocal != NULL)
  {
    sqlString = wxString::FromUTF8(sqlLocal);
    sqlite3_free(sqlLocal);
  }
#endif
  return sqlString;
}

void wxSQLite3Statement::Reset()
{
  if (m_stmt != NULL && m_stmt->m_isValid)
  {
    int rc = sqlite3_reset(m_stmt->m_stmt);

    if (rc != SQLITE_OK)
    {
      const char* localError = sqlite3_errmsg(m_db->m_db);
      throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }
  }
}

bool wxSQLite3Statement::IsReadOnly() const
{
#if SQLITE_VERSION_NUMBER >= 3007004
  CheckStmt();
  return sqlite3_stmt_readonly(m_stmt->m_stmt) != 0;
#else
  return false;
#endif
}

void wxSQLite3Statement::Finalize()
{
  Finalize(m_db, m_stmt);
  if (m_stmt != NULL && m_stmt->DecrementRefCount() == 0)
  {
    delete m_stmt;
  }
  m_stmt = NULL;
  if (m_db != NULL && m_db->DecrementRefCount() == 0)
  {
    if (m_db->m_isValid)
    {
      sqlite3_close(m_db->m_db);
    }
    delete m_db;
  }
  m_db = NULL;
}

void wxSQLite3Statement::Finalize(wxSQLite3DatabaseReference* db,wxSQLite3StatementReference* stmt)
{
  if (stmt != NULL && stmt->m_isValid)
  {
    int rc = sqlite3_finalize(stmt->m_stmt);
    stmt->Invalidate();
    if (rc != SQLITE_OK)
    {
      if (db != NULL && db->m_isValid)
      {
        const char* localError = sqlite3_errmsg(db->m_db);
        throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
      }
      else
      {
        throw wxSQLite3Exception(rc, wxERRMSG_FINALIZE_FAILED);
      }
    }
  }
}

bool wxSQLite3Statement::IsOk() const
{
  return (m_db != 0) && (m_stmt != 0);
}

bool wxSQLite3Statement::IsBusy() const
{
#if SQLITE_VERSION_NUMBER >= 3007010
  CheckStmt();
  int rc = sqlite3_stmt_busy(m_stmt->m_stmt);
  return (rc != 0);
#else
  return false;
#endif
}

int wxSQLite3Statement::Status(wxSQLite3StatementStatus opCode, bool resetFlag) const
{
  int count = 0;
#if SQLITE_VERSION_NUMBER >= 3007000
  CheckStmt();
  count = sqlite3_stmt_status(m_stmt->m_stmt, (int) opCode, (resetFlag) ? 1 : 0 );
#endif
  return count;
}

void wxSQLite3Statement::CheckDatabase() const
{
  if (m_db == NULL || m_db->m_db == NULL || !m_db->m_isValid)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NODB);
  }
}

void wxSQLite3Statement::CheckStmt() const
{
  if (m_stmt == NULL || m_stmt->m_stmt == NULL || !m_stmt->m_isValid)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOSTMT);
  }
}

//

wxSQLite3Blob::wxSQLite3Blob()
{
  m_db   = NULL;
  m_blob = NULL;
  m_writable = false;
}

wxSQLite3Blob::wxSQLite3Blob(const wxSQLite3Blob& blob)
{
  m_db   = blob.m_db;
  if (m_db != NULL)
  {
    m_db->IncrementRefCount();
  }
  m_blob = blob.m_blob;
  if (m_blob != NULL)
  {
    m_blob->IncrementRefCount();
  }
  m_writable = blob.m_writable;
}

wxSQLite3Blob& wxSQLite3Blob::operator=(const wxSQLite3Blob& blob)
{
  if (this != &blob)
  {
    wxSQLite3DatabaseReference* dbPrev = m_db;
    wxSQLite3BlobReference* blobPrev = m_blob;
    m_db   = blob.m_db;
    if (m_db != NULL)
    {
      m_db->IncrementRefCount();
    }
    m_blob = blob.m_blob;
    if (m_blob != NULL)
    {
      m_blob->IncrementRefCount();
    }
    m_writable = blob.m_writable;
    if (blobPrev != NULL && blobPrev->DecrementRefCount() == 0)
    {
      Finalize(dbPrev, blobPrev);
      delete blobPrev;
    }
    if (dbPrev != NULL && dbPrev->DecrementRefCount() == 0)
    {
      delete dbPrev;
    }
  }
  return *this;
}

wxSQLite3Blob::wxSQLite3Blob(wxSQLite3DatabaseReference* db, wxSQLite3BlobReference* blob, bool writable)
{
  m_db   = db;
  if (m_db != NULL)
  {
    m_db->IncrementRefCount();
  }
  m_blob = blob;
  if (m_blob != NULL)
  {
    m_blob->IncrementRefCount();
  }
  m_writable = writable;
}

wxSQLite3Blob::~wxSQLite3Blob()
{
  if (m_blob != NULL && m_blob->DecrementRefCount() == 0)
  {
    Finalize(m_db, m_blob);
    delete m_blob;
  }
  if (m_db != NULL && m_db->DecrementRefCount() == 0)
  {
    if (m_db->m_isValid)
    {
      sqlite3_close(m_db->m_db);
    }
    delete m_db;
  }
}

wxMemoryBuffer& wxSQLite3Blob::Read(wxMemoryBuffer& blobValue, int length, int offset) const
{
#if SQLITE_VERSION_NUMBER >= 3004000
  CheckBlob();
  char* localBuffer = (char*) blobValue.GetAppendBuf((size_t) length);
  int rc = sqlite3_blob_read(m_blob->m_blob, localBuffer, length, offset);

  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  blobValue.UngetAppendBuf((size_t) length);
#else
  wxUnusedVar(blobValue);
  wxUnusedVar(length);
  wxUnusedVar(offset);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOINCBLOB);
#endif
  return blobValue;
}

void wxSQLite3Blob::Write(const wxMemoryBuffer& blobValue, int offset)
{
#if SQLITE_VERSION_NUMBER >= 3004000
  CheckBlob();
  if (m_writable)
  {
    int blobLen = (int) blobValue.GetDataLen();
    int rc = sqlite3_blob_write(m_blob->m_blob,
                                (const void*) blobValue.GetData(), blobLen, offset);

    if (rc != SQLITE_OK)
    {
      const char* localError = sqlite3_errmsg(m_db->m_db);
      throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_BLOB);
  }
#else
  wxUnusedVar(blobValue);
  wxUnusedVar(offset);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOINCBLOB);
#endif
}

bool wxSQLite3Blob::IsOk() const
{
  return (m_blob != NULL && m_blob->m_isValid);
}

bool wxSQLite3Blob::IsReadOnly() const
{
  return !m_writable;
}

int wxSQLite3Blob::GetSize() const
{
#if SQLITE_VERSION_NUMBER >= 3004000
  CheckBlob();
  return sqlite3_blob_bytes(m_blob->m_blob);
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOINCBLOB);
  return 0;
#endif
}

void wxSQLite3Blob::Rebind(wxLongLong rowid)
{
#if SQLITE_VERSION_NUMBER >= 3007004
  CheckBlob();
  int rc = sqlite3_blob_reopen(m_blob->m_blob, rowid.GetValue());
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
#else
  wxUnusedVar(rowid);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOBLOBREBIND);
#endif
}

void wxSQLite3Blob::Finalize()
{
  Finalize(m_db, m_blob);
}

void wxSQLite3Blob::Finalize(wxSQLite3DatabaseReference* db, wxSQLite3BlobReference* blob)
{
#if SQLITE_VERSION_NUMBER >= 3004000
  if (blob != NULL && blob->m_isValid)
  {
    int rc = sqlite3_blob_close(blob->m_blob);
    blob->Invalidate();
    if (rc != SQLITE_OK)
    {
      if (db != NULL && db->m_isValid)
      {
        const char* localError = sqlite3_errmsg(db->m_db);
        throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
      }
      else
      {
        throw wxSQLite3Exception(rc, wxERRMSG_FINALIZE_FAILED);
      }
    }
  }
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOINCBLOB);
#endif
}

void wxSQLite3Blob::CheckBlob() const
{
  if (m_db == NULL || !m_db->m_isValid || m_blob == NULL || !m_blob->m_isValid)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_BLOB);
  }
}

// ----------------------------------------------------------------------------
// wxSQLite3Database: class holding a SQLite3 database object
// ----------------------------------------------------------------------------

bool wxSQLite3Database::ms_sharedCacheEnabled = false;

void
wxSQLite3Database::SetSharedCache(bool enable)
{
  int flag = (enable) ? 1 : 0;
  int rc = sqlite3_enable_shared_cache(flag);
  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_SHARED_CACHE);
  }
  ms_sharedCacheEnabled = enable;
}

bool wxSQLite3Database::ms_hasEncryptionSupport = true;

#if SQLITE_ENABLE_COLUMN_METADATA
bool wxSQLite3Database::ms_hasMetaDataSupport = true;
#else
bool wxSQLite3Database::ms_hasMetaDataSupport = false;
#endif

bool wxSQLite3Database::ms_hasUserAuthentication = false;

#if WXSQLITE3_HAVE_LOAD_EXTENSION
bool wxSQLite3Database::ms_hasLoadExtSupport = true;
#else
bool wxSQLite3Database::ms_hasLoadExtSupport = false;
#endif

#if WXSQLITE3_USE_NAMED_COLLECTIONS
bool wxSQLite3Database::ms_hasNamedCollectionSupport = true;
#else
bool wxSQLite3Database::ms_hasNamedCollectionSupport = false;
#endif

bool wxSQLite3Database::ms_hasIncrementalBlobSupport = true;

bool wxSQLite3Database::ms_hasSavepointSupport = true;

bool wxSQLite3Database::ms_hasBackupSupport = true;

bool wxSQLite3Database::ms_hasWriteAheadLogSupport = true;

bool wxSQLite3Database::ms_hasPointerParamsSupport = true;

bool
wxSQLite3Database::HasEncryptionSupport()
{
  return ms_hasEncryptionSupport;
}

bool
wxSQLite3Database::HasMetaDataSupport()
{
  return ms_hasMetaDataSupport;
}

bool
wxSQLite3Database::HasUserAuthenticationSupport()
{
  return ms_hasUserAuthentication;
}

bool
wxSQLite3Database::HasLoadExtSupport()
{
  return ms_hasLoadExtSupport;
}

bool
wxSQLite3Database::HasNamedCollectionSupport()
{
  return ms_hasNamedCollectionSupport;
}

bool
wxSQLite3Database::HasIncrementalBlobSupport()
{
  return ms_hasIncrementalBlobSupport;
}

bool
wxSQLite3Database::HasSavepointSupport()
{
  return ms_hasSavepointSupport;
}

bool
wxSQLite3Database::HasBackupSupport()
{
  return ms_hasBackupSupport;
}

bool
wxSQLite3Database::HasWriteAheadLogSupport()
{
  return ms_hasWriteAheadLogSupport;
}

bool
wxSQLite3Database::HasPointerParamsSupport()
{
  return ms_hasPointerParamsSupport;
}

wxSQLite3Database::wxSQLite3Database()
{
  m_db = 0;
  m_isOpen = false;
  m_busyTimeoutMs = 60000; // 60 seconds
  m_isEncrypted = false;
  m_lastRollbackRC = 0;
  m_backupPageCount = 10;
}

wxSQLite3Database::wxSQLite3Database(const wxSQLite3Database& db)
{
  m_db = db.m_db;
  if (m_db != NULL)
  {
    m_db->IncrementRefCount();
  }
  m_isOpen = db.m_isOpen;
  m_busyTimeoutMs = 60000; // 60 seconds
  m_isEncrypted = db.m_isEncrypted;
  m_lastRollbackRC = db.m_lastRollbackRC;
  m_backupPageCount = db.m_backupPageCount;
}

wxSQLite3Database::~wxSQLite3Database()
{
  if (m_db != NULL && m_db->DecrementRefCount() == 0)
  {
    if (m_db->m_isValid)
    {
      Close(m_db);
    }
    delete m_db;
  }
}

wxSQLite3Database& wxSQLite3Database::operator=(const wxSQLite3Database& db)
{
  if (this != &db)
  {
    wxSQLite3DatabaseReference* dbPrev = m_db;
    m_db = db.m_db;
    if (m_db != NULL)
    {
      m_db->IncrementRefCount();
      m_isOpen = db.m_isOpen;
      m_busyTimeoutMs = 60000; // 60 seconds
      m_isEncrypted = db.m_isEncrypted;
      m_lastRollbackRC = db.m_lastRollbackRC;
      m_backupPageCount = db.m_backupPageCount;
    }
    if (dbPrev != NULL && dbPrev->DecrementRefCount() == 0)
    {
      Close(dbPrev);
      delete dbPrev;
    }

    if (m_db == NULL)
    {
      throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_DBASSIGN_FAILED);
    }
  }
  return *this;
}

void wxSQLite3Database::Open(const wxString& fileName, const wxString& key, int flags, const wxString& vfs)
{
  wxCharBuffer strLocalKey = key.ToUTF8();
  const char* localKey = strLocalKey;
  wxMemoryBuffer binaryKey;
  if (key.Length() > 0)
  {
    binaryKey.AppendData((void*) localKey, strlen(localKey));
  }
  Open(fileName, binaryKey, flags, vfs);
}

void wxSQLite3Database::Open(const wxString& fileName, const wxMemoryBuffer& key, int flags, const wxString& vfs)
{
  wxCharBuffer strFileName = fileName.ToUTF8();
  const char* localFileName = strFileName;
  wxCharBuffer strVfs = vfs.ToUTF8();
  const char* localVfs = (!vfs.IsEmpty()) ? (const char*) strVfs : (const char*) NULL;
  sqlite3* db;

  int rc = sqlite3_open_v2((const char*) localFileName, &db, flags, localVfs);

  if (rc != SQLITE_OK)
  {
    const char* localError = "Out of memory";
    if (db != NULL)
    {
      localError = sqlite3_errmsg(db);
      sqlite3_close(db);
    }
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  rc = sqlite3_extended_result_codes(db, 1);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(db);
    sqlite3_close(db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  if (key.GetDataLen() > 0)
  {
    rc = sqlite3_key(db, key.GetData(), (int) key.GetDataLen());
    if (rc != SQLITE_OK)
    {
      const char* localError = sqlite3_errmsg(db);
      sqlite3_close(db);
      throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }
    m_isEncrypted = true;
  }

  wxSQLite3DatabaseReference* dbPrev = m_db;
  m_db = new wxSQLite3DatabaseReference(db);
  m_isOpen = true;
  SetBusyTimeout(m_busyTimeoutMs);
  if (dbPrev != NULL && dbPrev->DecrementRefCount() == 0)
  {
    delete dbPrev;
  }
}

void wxSQLite3Database::Open(const wxString& fileName, const wxSQLite3Cipher& cipher, const wxString& key, int flags, const wxString& vfs)
{
  wxCharBuffer strLocalKey = key.ToUTF8();
  const char* localKey = strLocalKey;
  wxMemoryBuffer binaryKey;
  if (key.Length() > 0)
  {
    binaryKey.AppendData((void*)localKey, strlen(localKey));
  }
  Open(fileName, cipher, binaryKey, flags, vfs);
}

void wxSQLite3Database::Open(const wxString& fileName, const wxSQLite3Cipher& cipher, const wxMemoryBuffer& key, int flags, const wxString& vfs)
{
  wxCharBuffer strFileName = fileName.ToUTF8();
  const char* localFileName = strFileName;
  wxCharBuffer strVfs = vfs.ToUTF8();
  const char* localVfs = (!vfs.IsEmpty()) ? (const char*) strVfs : (const char*) NULL;
  sqlite3* db;

  int rc = sqlite3_open_v2((const char*) localFileName, &db, flags, localVfs);

  if (rc != SQLITE_OK)
  {
    const char* localError = "Out of memory";
    if (db != NULL)
    {
      localError = sqlite3_errmsg(db);
      sqlite3_close(db);
    }
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  rc = sqlite3_extended_result_codes(db, 1);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(db);
    sqlite3_close(db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  if (cipher.IsOk())
  {
    if (!cipher.Apply(db))
    {
      throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_APPLY_FAILED);
    }
  }

  if (key.GetDataLen() > 0)
  {
    rc = sqlite3_key(db, key.GetData(), (int)key.GetDataLen());
    if (rc != SQLITE_OK)
    {
      const char* localError = sqlite3_errmsg(db);
      sqlite3_close(db);
      throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }
    m_isEncrypted = true;
  }

  wxSQLite3DatabaseReference* dbPrev = m_db;
  m_db = new wxSQLite3DatabaseReference(db);
  m_isOpen = true;
  SetBusyTimeout(m_busyTimeoutMs);
  if (dbPrev != NULL && dbPrev->DecrementRefCount() == 0)
  {
    delete dbPrev;
  }
}

bool wxSQLite3Database::IsOpen() const
{
  return (m_db != NULL && m_db->m_isValid && m_isOpen);
}

bool wxSQLite3Database::IsReadOnly(const wxString& databaseName) const
{
#if SQLITE_VERSION_NUMBER >= 3007011
  CheckDatabase();
  wxCharBuffer strDatabaseName = databaseName.ToUTF8();
  const char* localDatabaseName = strDatabaseName;
  return sqlite3_db_readonly(m_db->m_db, localDatabaseName) > 0;
#else
  return false;
#endif
}

void wxSQLite3Database::Close()
{
  CheckDatabase();
  Close(m_db);
}

void wxSQLite3Database::Close(wxSQLite3DatabaseReference* db)
{
  if (db != NULL && db->m_isValid)
  {
#if SQLITE_VERSION_NUMBER >= 3006000
// Unfortunately the following code leads to a crash if the RTree module is used
// therefore it is disabled for now
#if 0
    // Finalize all unfinalized prepared statements
    sqlite3_stmt *pStmt;
    while( (pStmt = sqlite3_next_stmt(db->m_db, 0))!=0 )
    {
      sqlite3_finalize(pStmt);
    }
#endif
#endif
    if (db->m_refCount <= 1)
    {
      sqlite3_close(db->m_db);
      db->Invalidate();
      m_isEncrypted = false;
    }
    m_isOpen = false;
  }
}

void wxSQLite3Database::AttachDatabase(const wxString& fileName, const wxString& schemaName)
{
  CheckDatabase();
  wxSQLite3Statement attachStmt = PrepareStatement("ATTACH DATABASE ? AS ?");
  attachStmt.Bind(1, fileName);
  attachStmt.Bind(2, schemaName);
  int rc = attachStmt.ExecuteUpdate();
}

void wxSQLite3Database::AttachDatabase(const wxString& fileName, const wxString& schemaName, const wxString& key)
{
  CheckDatabase();
  wxSQLite3Statement attachStmt = PrepareStatement("ATTACH DATABASE ? AS ? KEY ?");
  attachStmt.Bind(1, fileName);
  attachStmt.Bind(2, schemaName);
  attachStmt.Bind(3, key);
  int rc = attachStmt.ExecuteUpdate();
}

void wxSQLite3Database::AttachDatabase(const wxString& fileName, const wxString& schemaName, const wxSQLite3Cipher& cipher, const wxString& key)
{
  CheckDatabase();
  if (cipher.IsOk())
  {
    if (!cipher.Apply(m_db->m_db))
    {
      throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_APPLY_FAILED);
    }
  }

  wxSQLite3Statement attachStmt = PrepareStatement("ATTACH DATABASE ? AS ? KEY ?");
  attachStmt.Bind(1, fileName);
  attachStmt.Bind(2, schemaName);
  attachStmt.Bind(3, key);
  int rc = attachStmt.ExecuteUpdate();
}

void wxSQLite3Database::DetachDatabase(const wxString& schemaName)
{
  wxSQLite3Statement detachStmt = PrepareStatement("DETACH DATABASE ?");
  detachStmt.Bind(1, schemaName);
  int rc = detachStmt.ExecuteUpdate();
}

static bool
BackupRestoreCallback(int total, int remaining, wxSQLite3BackupProgress* progressCallback)
{
  return progressCallback->Progress(total, remaining);
}

void wxSQLite3Database::Backup(const wxString& targetFileName, const wxString& key, 
                               const wxString& sourceDatabaseName)
{
  Backup(NULL, targetFileName, wxSQLite3Cipher(), key, sourceDatabaseName);
}

void wxSQLite3Database::Backup(const wxString& targetFileName, const wxSQLite3Cipher& cipher, 
                               const wxString& key, const wxString& sourceDatabaseName)
{
  Backup(NULL, targetFileName, cipher, key, sourceDatabaseName);
}

void wxSQLite3Database::Backup(wxSQLite3BackupProgress* progressCallback,
                               const wxString& targetFileName, const wxString& key,
                               const wxString& sourceDatabaseName)
{
  Backup(progressCallback, targetFileName, wxSQLite3Cipher(), key, sourceDatabaseName);
}

void wxSQLite3Database::Backup(wxSQLite3BackupProgress* progressCallback, 
                               const wxString& targetFileName, const wxSQLite3Cipher& cipher, 
                               const wxString& key, const wxString& sourceDatabaseName)
{
  wxCharBuffer strLocalKey = key.ToUTF8();
  const char* localKey = strLocalKey;
  wxMemoryBuffer binaryKey;
  if (key.Length() > 0)
  {
    binaryKey.AppendData((void*) localKey, strlen(localKey));
  }
  Backup(progressCallback, targetFileName, cipher, binaryKey, sourceDatabaseName);
}

void wxSQLite3Database::Backup(const wxString& targetFileName, const wxMemoryBuffer& key, 
                               const wxString& sourceDatabaseName)
{
  Backup(NULL, targetFileName, wxSQLite3Cipher(), key, sourceDatabaseName);
}

void wxSQLite3Database::Backup(const wxString& targetFileName, const wxSQLite3Cipher& cipher, 
                               const wxMemoryBuffer& key, const wxString& sourceDatabaseName)
{
  Backup(NULL, targetFileName, cipher, key, sourceDatabaseName);
}

void wxSQLite3Database::Backup(wxSQLite3BackupProgress* progressCallback, const wxString& targetFileName,
                               const wxMemoryBuffer& key, const wxString& sourceDatabaseName)
{
  Backup(progressCallback, targetFileName, wxSQLite3Cipher(), key, sourceDatabaseName);
}

void wxSQLite3Database::Backup(wxSQLite3BackupProgress* progressCallback,
                               const wxString& targetFileName, const wxSQLite3Cipher& cipher, 
                               const wxMemoryBuffer& key, const wxString& sourceDatabaseName)
{
  CheckDatabase();

  wxCharBuffer strFileName = targetFileName.ToUTF8();
  const char* localTargetFileName = strFileName;
  wxCharBuffer strDatabaseName = sourceDatabaseName.ToUTF8();
  const char* localSourceDatabaseName = strDatabaseName;

  sqlite3* pDest;
  sqlite3_backup* pBackup;
  int rc;
  rc = sqlite3_open(localTargetFileName, &pDest);
  if (rc != SQLITE_OK)
  {
    sqlite3_close(pDest);
    throw wxSQLite3Exception(rc, wxERRMSG_DBOPEN_FAILED);
  }

  if (key.GetDataLen() > 0)
  {
    if (cipher.IsOk())
    {
      if (!cipher.Apply(pDest))
      {
        throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_APPLY_FAILED);
      }
    }
    rc = sqlite3_key(pDest, key.GetData(), (int) key.GetDataLen());
    if (rc != SQLITE_OK)
    {
      const char* localError = sqlite3_errmsg(pDest);
      sqlite3_close(pDest);
      throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }
  }

  pBackup = sqlite3_backup_init(pDest, "main", m_db->m_db, localSourceDatabaseName);
  if (pBackup == 0)
  {
    const char* localError = sqlite3_errmsg(pDest);
    sqlite3_close(pDest);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  do
  {
    rc = sqlite3_backup_step(pBackup, m_backupPageCount);
    if (progressCallback != NULL)
    {
      if (!BackupRestoreCallback(sqlite3_backup_pagecount(pBackup),
                                 sqlite3_backup_remaining(pBackup),
                                 progressCallback))
      {
        rc = SQLITE_DONE;
      }
    }
    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
    {
      sqlite3_sleep(250);
    }
  }
  while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

  sqlite3_backup_finish(pBackup);
  if (rc == SQLITE_DONE)
  {
    sqlite3_close(pDest);
  }
  else
  {
    const char* localError = sqlite3_errmsg(pDest);
    sqlite3_close(pDest);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

void wxSQLite3Database::Restore(const wxString& sourceFileName, const wxString& key, 
                                const wxString& targetDatabaseName)
{
  Restore(NULL, sourceFileName, wxSQLite3Cipher(), key, targetDatabaseName);
}

void wxSQLite3Database::Restore(const wxString& sourceFileName, const wxSQLite3Cipher& cipher,
                                const wxString& key, const wxString& targetDatabaseName)
{
  Restore(NULL, sourceFileName, cipher, key, targetDatabaseName);
}

void wxSQLite3Database::Restore(wxSQLite3BackupProgress* progressCallback,
                                const wxString& sourceFileName, const wxString& key,
                                const wxString& targetDatabaseName)
{
  Restore(progressCallback, sourceFileName, wxSQLite3Cipher(), key, targetDatabaseName);
}

void wxSQLite3Database::Restore(wxSQLite3BackupProgress* progressCallback,
                                const wxString& sourceFileName, const wxSQLite3Cipher& cipher,
                                const wxString& key, const wxString& targetDatabaseName)
{
  wxCharBuffer strLocalKey = key.ToUTF8();
  const char* localKey = strLocalKey;
  wxMemoryBuffer binaryKey;
  if (key.Length() > 0)
  {
    binaryKey.AppendData((void*) localKey, strlen(localKey));
  }
  Restore(progressCallback, sourceFileName, cipher, binaryKey, targetDatabaseName);
}

void wxSQLite3Database::Restore(const wxString& sourceFileName, const wxMemoryBuffer& key, 
                                const wxString& targetDatabaseName)
{
  Restore(NULL, sourceFileName, wxSQLite3Cipher(), key, targetDatabaseName);
}

void wxSQLite3Database::Restore(wxSQLite3BackupProgress* progressCallback, const wxString& sourceFileName,
                                const wxMemoryBuffer& key, const wxString& targetDatabaseName)
{
  Restore(progressCallback, sourceFileName, wxSQLite3Cipher(), key, targetDatabaseName);
}

void wxSQLite3Database::Restore(const wxString& sourceFileName, const wxSQLite3Cipher& cipher,
                                const wxMemoryBuffer& key, const wxString& targetDatabaseName)
{
    Restore(NULL, sourceFileName, cipher, key, targetDatabaseName);
}

void wxSQLite3Database::Restore(wxSQLite3BackupProgress* progressCallback,
                                const wxString& sourceFileName, const wxSQLite3Cipher& cipher,
                                const wxMemoryBuffer& key, const wxString& targetDatabaseName)
{
  CheckDatabase();

  wxCharBuffer strFileName = sourceFileName.ToUTF8();
  const char* localSourceFileName = strFileName;
  wxCharBuffer strDatabaseName = targetDatabaseName.ToUTF8();
  const char* localTargetDatabaseName = strDatabaseName;

  sqlite3* pSrc;
  sqlite3_backup* pBackup;
  int rc;
  int nTimeout = 0;

  rc = sqlite3_open(localSourceFileName, &pSrc);
  if (rc != SQLITE_OK)
  {
    sqlite3_close(pSrc);
    throw wxSQLite3Exception(rc, wxERRMSG_DBOPEN_FAILED);
  }

  if (key.GetDataLen() > 0)
  {
    if (cipher.IsOk())
    {
      if (!cipher.Apply(pSrc))
      {
        throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_APPLY_FAILED);
      }
    }
    rc = sqlite3_key(pSrc, key.GetData(), (int) key.GetDataLen());
    if (rc != SQLITE_OK)
    {
      const char* localError = sqlite3_errmsg(pSrc);
      sqlite3_close(pSrc);
      throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }
  }

  pBackup = sqlite3_backup_init(m_db->m_db, localTargetDatabaseName, pSrc, "main");
  if (pBackup == 0)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    sqlite3_close(pSrc);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  do
  {
    rc = sqlite3_backup_step(pBackup, m_backupPageCount);
    if (progressCallback != NULL)
    {
      if (!BackupRestoreCallback(sqlite3_backup_pagecount(pBackup),
                                 sqlite3_backup_remaining(pBackup), progressCallback))
      {
        rc = SQLITE_DONE;
      }
    }
    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
    {
      if (nTimeout++ >= 20) break;
      sqlite3_sleep(250);
    }
    else
    {
      nTimeout = 0;
    }
  }
  while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

  sqlite3_backup_finish(pBackup);
  if (rc == SQLITE_DONE)
  {
    sqlite3_close(pSrc);
  }
  else if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
  {
    sqlite3_close(pSrc);
    throw wxSQLite3Exception(rc, wxERRMSG_SOURCEDB_BUSY);
  }
  else
  {
    const char* localError = sqlite3_errmsg(pSrc);
    sqlite3_close(pSrc);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

void wxSQLite3Database::SetBackupRestorePageCount(int pageCount)
{
  m_backupPageCount = pageCount;
}

void wxSQLite3Database::Vacuum()
{
  ExecuteUpdate("vacuum");
}

void wxSQLite3Database::Begin(wxSQLite3TransactionType transactionType)
{
  wxString sql;
  switch (transactionType)
  {
    case WXSQLITE_TRANSACTION_DEFERRED:
      sql << wxS("begin deferred transaction");
      break;
    case WXSQLITE_TRANSACTION_IMMEDIATE:
      sql << wxS("begin immediate transaction");
      break;
    case WXSQLITE_TRANSACTION_EXCLUSIVE:
      sql << wxS("begin exclusive transaction");
      break;
    default:
      sql << wxS("begin transaction");
      break;
  }
  ExecuteUpdate(sql);
}

void wxSQLite3Database::Commit()
{
  ExecuteUpdate("commit transaction");
}

void wxSQLite3Database::Rollback(const wxString& savepointName)
{
#if SQLITE_VERSION_NUMBER >= 3006008
  if (savepointName.IsEmpty())
  {
#endif
    ExecuteUpdate("rollback transaction");
#if SQLITE_VERSION_NUMBER >= 3006008
  }
  else
  {
    wxString localSavepointName = savepointName;
    localSavepointName.Replace(wxString(wxS("\"")), wxString(wxS("\"\"")));
    ExecuteUpdate(wxString(wxS("rollback transaction to savepoint \""))+localSavepointName+wxString(wxS("\"")));
  }
#endif
}

bool wxSQLite3Database::GetAutoCommit() const
{
  CheckDatabase();
  return sqlite3_get_autocommit(m_db->m_db) != 0;
}

int wxSQLite3Database::QueryRollbackState() const
{
  return m_lastRollbackRC;
}

wxSQLite3TransactionState wxSQLite3Database::QueryTransactionState(const wxString& schemaName) const
{
  wxSQLite3TransactionState state = WXSQLITE_TRANSACTION_NONE;
  int txnState;
  CheckDatabase();
  if (schemaName.IsEmpty())
  {
    txnState = sqlite3_txn_state(m_db->m_db, NULL);
  }
  else
  {
    wxCharBuffer strSchema = schemaName.ToUTF8();
    const char* localSchema = strSchema;
    txnState = sqlite3_txn_state(m_db->m_db, localSchema);
  }
  if (txnState < 0)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_SCHEMANAME_UNKNOWN);
  }
  else
  {
    switch (txnState)
    {
      case SQLITE_TXN_READ:
        state = WXSQLITE_TRANSACTION_READ;
        break;
      case SQLITE_TXN_WRITE:
        state = WXSQLITE_TRANSACTION_WRITE;
        break;
      case SQLITE_TXN_NONE:
      default:
        state = WXSQLITE_TRANSACTION_NONE;
        break;
    }
  }
  return state;
}

void wxSQLite3Database::Savepoint(const wxString& savepointName)
{
#if SQLITE_VERSION_NUMBER >= 3006008
  wxString localSavepointName = savepointName;
  localSavepointName.Replace(wxString(wxS("\"")), wxString(wxS("\"\"")));
  ExecuteUpdate(wxString(wxS("savepoint \"")) + localSavepointName + wxString(wxS("\"")));
#else
  wxUnusedVar(savepointName);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOSAVEPOINT);
#endif
}

void wxSQLite3Database::ReleaseSavepoint(const wxString& savepointName)
{
#if SQLITE_VERSION_NUMBER >= 3006008
  wxString localSavepointName = savepointName;
  localSavepointName.Replace(wxString(wxS("\"")), wxString(wxS("\"\"")));
  ExecuteUpdate(wxString(wxS("release savepoint \"")) + localSavepointName + wxString(wxS("\"")));
#else
  wxUnusedVar(savepointName);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOSAVEPOINT);
#endif
}

wxSQLite3Statement wxSQLite3Database::PrepareStatement(const wxString& sql)
{
  wxCharBuffer strSql = sql.ToUTF8();
  const char* localSql = strSql;
  return PrepareStatement(localSql);
}

wxSQLite3Statement wxSQLite3Database::PrepareStatement(const wxSQLite3StatementBuffer& sql)
{
  return PrepareStatement((const char*) sql);
}

wxSQLite3Statement wxSQLite3Database::PrepareStatement(const char* sql)
{
  CheckDatabase();
  sqlite3_stmt* stmt = (sqlite3_stmt*) Prepare(sql);
  wxSQLite3StatementReference* stmtRef = new wxSQLite3StatementReference(stmt);
  return wxSQLite3Statement(m_db, stmtRef);
}

wxSQLite3Statement wxSQLite3Database::PreparePersistentStatement(const wxString& sql)
{
  wxCharBuffer strSql = sql.ToUTF8();
  const char* localSql = strSql;
#if SQLITE_VERSION_NUMBER >= 3020000
  return PreparePersistentStatement(localSql);
#else
  return PrepareStatement(localSql);
#endif
}

wxSQLite3Statement wxSQLite3Database::PreparePersistentStatement(const wxSQLite3StatementBuffer& sql)
{
#if SQLITE_VERSION_NUMBER >= 3020000
  return PreparePersistentStatement((const char*) sql);
#else
  return PrepareStatement((const char*)sql);
#endif
}

wxSQLite3Statement wxSQLite3Database::PreparePersistentStatement(const char* sql)
{
  CheckDatabase();
#if SQLITE_VERSION_NUMBER >= 3020000
  sqlite3_stmt* stmt = (sqlite3_stmt*) PreparePersistent(sql);
#else
  sqlite3_stmt* stmt = (sqlite3_stmt*)Prepare(sql);
#endif
  wxSQLite3StatementReference* stmtRef = new wxSQLite3StatementReference(stmt);
  return wxSQLite3Statement(m_db, stmtRef);
}

bool wxSQLite3Database::TableExists(const wxString& tableName, const wxString& databaseName)
{
  wxString sql;
  if (databaseName.IsEmpty())
  {
    sql = wxS("select count(*) from sqlite_master where type='table' and name like ?");
  }
  else
  {
    sql = wxString(wxS("select count(*) from ")) + databaseName + wxString(wxS(".sqlite_master where type='table' and name like ?"));
  }
  wxSQLite3Statement stmt = PrepareStatement(sql);
  stmt.Bind(1, tableName);
  wxSQLite3ResultSet resultSet = stmt.ExecuteQuery();
  long value = 0;
  resultSet.GetAsString(0).ToLong(&value);
  return (value > 0);
}

bool wxSQLite3Database::TableExists(const wxString& tableName, wxArrayString& databaseNames)
{
  wxArrayString databaseList;
  GetDatabaseList(databaseList);

  bool found = false;
  size_t count = databaseList.GetCount();
  if (count > 0)
  {
    size_t j;
    for (j = 0; j < count; j++)
    {
      if (TableExists(tableName, databaseList.Item(j)))
      {
        found = true;
        databaseNames.Add(databaseList.Item(j));
      }
    }
  }
  return found;
}

void wxSQLite3Database::GetDatabaseList(wxArrayString& databaseNames)
{
  databaseNames.Empty();
  wxSQLite3ResultSet resultSet = ExecuteQuery("PRAGMA database_list;");
  while (resultSet.NextRow())
  {
    databaseNames.Add(resultSet.GetString(1));
  }
}

void wxSQLite3Database::GetDatabaseList(wxArrayString& databaseNames, wxArrayString& databaseFiles)
{
  databaseNames.Empty();
  databaseFiles.Empty();
  wxSQLite3ResultSet resultSet = ExecuteQuery("PRAGMA database_list;");
  while (resultSet.NextRow())
  {
    databaseNames.Add(resultSet.GetString(1));
    databaseFiles.Add(resultSet.GetString(2));
  }
}

wxString wxSQLite3Database::GetDatabaseFilename(const wxString& databaseName)
{
#if SQLITE_VERSION_NUMBER >= 3007010
  CheckDatabase();
  wxCharBuffer strDatabaseName = databaseName.ToUTF8();
  const char* localDatabaseName = strDatabaseName;
  const char* localFilename = sqlite3_db_filename(m_db->m_db, localDatabaseName);
  return wxString::FromUTF8(localFilename);
#else
  wxUnusedVar(databaseName);
  return wxEmptyString;
#endif
}

bool wxSQLite3Database::EnableForeignKeySupport(bool enable)
{
  if (enable)
  {
    ExecuteUpdate("PRAGMA foreign_keys=ON;");
  }
  else
  {
    ExecuteUpdate("PRAGMA foreign_keys=OFF;");
  }
  bool enabled = IsForeignKeySupportEnabled();
  return (enable && enabled) || (!enable && !enabled);
}

bool wxSQLite3Database::IsForeignKeySupportEnabled()
{
  bool enabled = false;
  wxSQLite3ResultSet resultSet = ExecuteQuery("PRAGMA foreign_keys;");
  if (resultSet.NextRow())
  {
    enabled = (resultSet.GetInt(0) == 1);
  }
  return enabled;
}

wxSQLite3JournalMode
wxSQLite3Database::SetJournalMode(wxSQLite3JournalMode journalMode, const wxString& database)
{
  wxString mode = ConvertJournalMode(journalMode);
  wxString query = wxS("PRAGMA ");
  if (!database.IsEmpty())
  {
    query += database;
    query += wxS(".");
  }
  query += wxS("journal_mode=");
  query += mode;
  query += wxS(";");
  wxSQLite3ResultSet resultSet = ExecuteQuery(query);
  if (resultSet.NextRow())
  {
    mode = resultSet.GetString(0);
  }
  return ConvertJournalMode(mode);
}

wxSQLite3JournalMode
wxSQLite3Database::GetJournalMode(const wxString& database)
{
  wxString mode = wxS("DELETE");
  wxString query = wxS("PRAGMA ");
  if (!database.IsEmpty())
  {
    query += database;
    query += wxS(".");
  }
  query += wxS("journal_mode;");
  wxSQLite3ResultSet resultSet = ExecuteQuery(query);
  if (resultSet.NextRow())
  {
    mode = resultSet.GetString(0);
  }
  return ConvertJournalMode(mode);
}

/* static */
wxString wxSQLite3Database::ConvertJournalMode(wxSQLite3JournalMode mode)
{
  wxString journalMode;
  if      (mode == WXSQLITE_JOURNALMODE_DELETE)   journalMode = wxS("DELETE");
  else if (mode == WXSQLITE_JOURNALMODE_PERSIST)  journalMode = wxS("PERSIST");
  else if (mode == WXSQLITE_JOURNALMODE_OFF)      journalMode = wxS("OFF");
  else if (mode == WXSQLITE_JOURNALMODE_TRUNCATE) journalMode = wxS("TRUNCATE");
  else if (mode == WXSQLITE_JOURNALMODE_MEMORY)   journalMode = wxS("MEMORY");
  else if (mode == WXSQLITE_JOURNALMODE_WAL)      journalMode = wxS("WAL");
  else                                            journalMode = wxS("DELETE");
  return journalMode;
}

/* static */
wxSQLite3JournalMode wxSQLite3Database::ConvertJournalMode(const wxString& mode)
{
  wxSQLite3JournalMode journalMode;
  if      (mode.IsSameAs(wxS("DELETE"), false))   journalMode = WXSQLITE_JOURNALMODE_DELETE;
  else if (mode.IsSameAs(wxS("PERSIST"), false))  journalMode = WXSQLITE_JOURNALMODE_PERSIST;
  else if (mode.IsSameAs(wxS("OFF"), false))      journalMode = WXSQLITE_JOURNALMODE_OFF;
  else if (mode.IsSameAs(wxS("TRUNCATE"), false)) journalMode = WXSQLITE_JOURNALMODE_TRUNCATE;
  else if (mode.IsSameAs(wxS("MEMORY"), false))   journalMode = WXSQLITE_JOURNALMODE_MEMORY;
  else if (mode.IsSameAs(wxS("WAL"), false))      journalMode = WXSQLITE_JOURNALMODE_WAL;
  else                                            journalMode = WXSQLITE_JOURNALMODE_DELETE;
  return journalMode;
}

bool wxSQLite3Database::CheckSyntax(const wxString& sql) const
{
  wxCharBuffer strSql = sql.ToUTF8();
  const char* localSql = strSql;
  return CheckSyntax(localSql);
}

bool wxSQLite3Database::CheckSyntax(const wxSQLite3StatementBuffer& sql) const
{
  return CheckSyntax((const char*) sql);
}

bool wxSQLite3Database::CheckSyntax(const char* sql) const
{
  return sqlite3_complete(sql) != 0;
}

int wxSQLite3Database::ExecuteUpdate(const wxString& sql)
{
  wxCharBuffer strSql = sql.ToUTF8();
  const char* localSql = strSql;
  return ExecuteUpdate(localSql);
}

int wxSQLite3Database::ExecuteUpdate(const wxSQLite3StatementBuffer& sql)
{
  return ExecuteUpdate((const char*) sql);
}

int wxSQLite3Database::ExecuteUpdate(const char* sql, bool saveRC)
{
  CheckDatabase();

  char* localError = 0;

  int rc = sqlite3_exec(m_db->m_db, sql, 0, 0, &localError);
  if (saveRC)
  {
    if (strncmp(sql, "rollback transaction", 20) == 0)
    {
      m_lastRollbackRC = rc;
    }
  }

  if (rc == SQLITE_OK)
  {
    return sqlite3_changes(m_db->m_db);
  }
  else
  {
    wxString errmsg = wxString::FromUTF8(localError);
    sqlite3_free(localError);
    throw wxSQLite3Exception(rc, errmsg);
  }
}

wxSQLite3ResultSet wxSQLite3Database::ExecuteQuery(const wxString& sql)
{
  wxCharBuffer strSql = sql.ToUTF8();
  const char* localSql = strSql;
  return ExecuteQuery(localSql);
}

wxSQLite3ResultSet wxSQLite3Database::ExecuteQuery(const wxSQLite3StatementBuffer& sql)
{
  return ExecuteQuery((const char*) sql);
}

wxSQLite3ResultSet wxSQLite3Database::ExecuteQuery(const char* sql)
{
  CheckDatabase();

  sqlite3_stmt* stmt = (sqlite3_stmt*) Prepare(sql);

  int rc = sqlite3_step(stmt);

  if (rc == SQLITE_DONE) // no rows
  {
    wxSQLite3StatementReference* stmtRef = new wxSQLite3StatementReference(stmt);
    return wxSQLite3ResultSet(m_db, stmtRef, true /* eof */);
  }
  else if (rc == SQLITE_ROW) // one or more rows
  {
    wxSQLite3StatementReference* stmtRef = new wxSQLite3StatementReference(stmt);
    return wxSQLite3ResultSet(m_db, stmtRef, false /* eof */);
  }
  else
  {
    rc = sqlite3_finalize(stmt);
    const char* localError= sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

int wxSQLite3Database::ExecuteScalar(const wxString& sql)
{
  wxCharBuffer strSql = sql.ToUTF8();
  const char* localSql = strSql;
  return ExecuteScalar(localSql);
}

int wxSQLite3Database::ExecuteScalar(const wxSQLite3StatementBuffer& sql)
{
  return ExecuteScalar((const char*) sql);
}

int wxSQLite3Database::ExecuteScalar(const char* sql)
{
  wxSQLite3ResultSet resultSet = ExecuteQuery(sql);

  if (resultSet.Eof() || resultSet.GetColumnCount() < 1)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_QUERY);
  }

  long value = 0;
  resultSet.GetAsString(0).ToLong(&value);
  return (int) value;
}

wxSQLite3Table wxSQLite3Database::GetTable(const wxString& sql)
{
  wxCharBuffer strSql = sql.ToUTF8();
  const char* localSql = strSql;
  return GetTable(localSql);
}

wxSQLite3Table wxSQLite3Database::GetTable(const wxSQLite3StatementBuffer& sql)
{
  return GetTable((const char*) sql);
}

wxSQLite3Table wxSQLite3Database::GetTable(const char* sql)
{
  CheckDatabase();

  char* localError=0;
  char** results=0;
  int rc;
  int rows(0);
  int cols(0);

  rc = sqlite3_get_table(m_db->m_db, sql, &results, &rows, &cols, &localError);

  if (rc == SQLITE_OK)
  {
    return wxSQLite3Table(results, rows, cols);
  }
  else
  {
    wxString errmsg = wxString::FromUTF8(localError);
    sqlite3_free(localError);
    throw wxSQLite3Exception(rc, errmsg);
  }
}

wxLongLong wxSQLite3Database::GetLastRowId() const
{
  CheckDatabase();
  return wxLongLong(sqlite3_last_insert_rowid(m_db->m_db));
}

wxSQLite3Blob wxSQLite3Database::GetReadOnlyBlob(wxLongLong rowId,
                                                 const wxString& columnName,
                                                 const wxString& tableName,
                                                 const wxString& dbName)
{
  return GetBlob(rowId, columnName, tableName, dbName, false);
}

wxSQLite3Blob wxSQLite3Database::GetWritableBlob(wxLongLong rowId,
                                                 const wxString& columnName,
                                                 const wxString& tableName,
                                                 const wxString& dbName)
{
  return GetBlob(rowId, columnName, tableName, dbName, true);
}

wxSQLite3Blob wxSQLite3Database::GetBlob(wxLongLong rowId,
                                         const wxString& columnName,
                                         const wxString& tableName,
                                         const wxString& dbName,
                                         bool writable)
{
#if SQLITE_VERSION_NUMBER >= 3004000
  wxCharBuffer strColumnName = columnName.ToUTF8();
  const char* localColumnName = strColumnName;
  wxCharBuffer strTableName = tableName.ToUTF8();
  const char* localTableName = strTableName;
  wxCharBuffer strDbName = dbName.ToUTF8();
  const char* localDbName = (!dbName.IsEmpty()) ? (const char*) strDbName : (const char*) NULL;
  int flags = (writable) ? 1 : 0;
  sqlite3_blob* blobHandle;
  CheckDatabase();
  int rc = sqlite3_blob_open(m_db->m_db, localDbName, localTableName, localColumnName, rowId.GetValue(), flags, &blobHandle);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
  wxSQLite3BlobReference* blobRef = new wxSQLite3BlobReference(blobHandle);
  return wxSQLite3Blob(m_db, blobRef, writable);
#else
  wxUnusedVar(rowId);
  wxUnusedVar(columnName);
  wxUnusedVar(tableName);
  wxUnusedVar(dbName);
  wxUnusedVar(writable);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOINCBLOB);
  return NULL;
#endif
}

void wxSQLite3Database::Interrupt()
{
  CheckDatabase();
  sqlite3_interrupt(m_db->m_db);
}

bool wxSQLite3Database::IsInterrupted()
{
  CheckDatabase();
  return (sqlite3_is_interrupted(m_db->m_db) != 0);
}

void wxSQLite3Database::SetBusyTimeout(int nMillisecs)
{
  CheckDatabase();
  m_busyTimeoutMs = nMillisecs;
  sqlite3_busy_timeout(m_db->m_db, m_busyTimeoutMs);
}

void wxSQLite3Database::SetLockTimeout(int nMillisecs, bool blockOnConnect)
{
#if SQLITE_VERSION_NUMBER >= 3050000
  CheckDatabase();
  int rc = sqlite3_setlk_timeout(m_db->m_db, nMillisecs, (blockOnConnect) ? 1 : 0);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
#else
  wxUnusedVar(nMillisecs);
  wxUnusedVar(blockOnConnect);
#endif
}

void wxSQLite3Database::Configure(wxSQLite3DbConfig cfgType, int cfgValue, int& cfgResult)
{
  CheckDatabase();
  int rc = SQLITE_ERROR;
  int localCfgType = static_cast<int>(cfgType);
  if (localCfgType >= SQLITE_DBCONFIG_ENABLE_FKEY && localCfgType <= SQLITE_DBCONFIG_ENABLE_COMMENTS)
  {
    rc = sqlite3_db_config(m_db->m_db, cfgType, cfgValue, &cfgResult);
    if (rc != SQLITE_OK)
    {
        const char* localError = sqlite3_errmsg(m_db->m_db);
        throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
    }
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_DBCONFIG_OPTION_UNKNOWN);
  }
}

wxString wxSQLite3Database::GetWrapperVersion()
{
  return wxString(wxSQLITE3_VERSION_STRING);
}

wxString wxSQLite3Database::GetVersion()
{
  return wxString::FromUTF8(sqlite3_libversion());
}

wxString wxSQLite3Database::GetSourceId()
{
#if SQLITE_VERSION_NUMBER >= 3006018
  return wxString::FromUTF8(sqlite3_sourceid());
#else
  return wxEmptyString;
#endif
}

bool wxSQLite3Database::CompileOptionUsed(const wxString& optionName)
{
#if SQLITE_VERSION_NUMBER >= 3006023
  wxCharBuffer strOption = optionName.ToUTF8();
  const char* localOption = strOption;
  return sqlite3_compileoption_used(localOption) == 1;
#else
  return false;
#endif
}

wxString wxSQLite3Database::GetCompileOptionName(int optionIndex)
{
#if SQLITE_VERSION_NUMBER >= 3006023
  const char* unknownOption = "";
  const char* optionName = sqlite3_compileoption_get(optionIndex);
  if (optionName == NULL)
  {
    optionName = unknownOption;
  }
  return wxString::FromUTF8(optionName);
#else
  return wxEmptyString;
#endif
}

bool wxSQLite3Database::CreateFunction(const wxString& funcName, int argCount, wxSQLite3ScalarFunction& function, int functionFlags)
{
  CheckDatabase();
  wxCharBuffer strFuncName = funcName.ToUTF8();
  const char* localFuncName = strFuncName;
  int flags = SQLITE_UTF8;
#if SQLITE_VERSION_NUMBER >= 3008003
  flags |= functionFlags;
#endif
  int rc = sqlite3_create_function(m_db->m_db, localFuncName, argCount,
                                   flags, &function,
                                   (void (*)(sqlite3_context*,int,sqlite3_value**)) wxSQLite3FunctionContext::ExecScalarFunction, NULL, NULL);
  return rc == SQLITE_OK;
}

bool wxSQLite3Database::CreateFunction(const wxString& funcName, int argCount, wxSQLite3AggregateFunction& function, int functionFlags)
{
  CheckDatabase();
  wxCharBuffer strFuncName = funcName.ToUTF8();
  const char* localFuncName = strFuncName;
  int flags = SQLITE_UTF8;
#if SQLITE_VERSION_NUMBER >= 3008003
  flags |= functionFlags;
#endif
  int rc = sqlite3_create_function(m_db->m_db, localFuncName, argCount,
                                   flags, &function,
                                   NULL,
                                   (void (*)(sqlite3_context*,int,sqlite3_value**)) wxSQLite3FunctionContext::ExecAggregateStep,
                                   (void (*)(sqlite3_context*)) wxSQLite3FunctionContext::ExecAggregateFinalize);
  return rc == SQLITE_OK;
}

bool wxSQLite3Database::CreateFunction(const wxString& funcName, int argCount, wxSQLite3WindowFunction& function, int functionFlags)
{
#if SQLITE_VERSION_NUMBER >= 3025000
  CheckDatabase();
  wxCharBuffer strFuncName = funcName.ToUTF8();
  const char* localFuncName = strFuncName;
  int flags = SQLITE_UTF8 | functionFlags;
  int rc = sqlite3_create_window_function(m_db->m_db, localFuncName, argCount,
                                          flags, &function,
                                          (void(*)(sqlite3_context*, int, sqlite3_value**)) wxSQLite3FunctionContext::ExecWindowStep,
                                          (void(*)(sqlite3_context*)) wxSQLite3FunctionContext::ExecWindowFinalize,
                                          (void(*)(sqlite3_context*)) wxSQLite3FunctionContext::ExecWindowValue,
                                          (void(*)(sqlite3_context*, int, sqlite3_value**)) wxSQLite3FunctionContext::ExecWindowInverse,
                                          NULL);
  return rc == SQLITE_OK;
#else
  return false;
#endif
}

bool wxSQLite3Database::SetAuthorizer(wxSQLite3Authorizer& authorizer)
{
  CheckDatabase();
  int rc = sqlite3_set_authorizer(m_db->m_db, (sqlite3_xauth) wxSQLite3FunctionContextExecAuthorizer, &authorizer);
  return rc == SQLITE_OK;
}

bool wxSQLite3Database::RemoveAuthorizer()
{
  CheckDatabase();
  int rc = sqlite3_set_authorizer(m_db->m_db, (sqlite3_xauth) NULL, NULL);
  return rc == SQLITE_OK;
}

void wxSQLite3Database::SetCommitHook(wxSQLite3Hook* commitHook)
{
  CheckDatabase();
  if (commitHook)
  {
    sqlite3_commit_hook(m_db->m_db, (int(*)(void*)) wxSQLite3FunctionContext::ExecCommitHook, commitHook);
  }
  else
  {
    sqlite3_commit_hook(m_db->m_db, (int(*)(void*)) NULL, NULL);
  }
}

void wxSQLite3Database::SetRollbackHook(wxSQLite3Hook* rollbackHook)
{
  CheckDatabase();
  if (rollbackHook)
  {
    sqlite3_rollback_hook(m_db->m_db, (void(*)(void*)) wxSQLite3FunctionContext::ExecRollbackHook, rollbackHook);
  }
  else
  {
    sqlite3_rollback_hook(m_db->m_db, (void(*)(void*)) NULL, NULL);
  }
}

void wxSQLite3Database::SetUpdateHook(wxSQLite3Hook* updateHook)
{
  CheckDatabase();
  if (updateHook)
  {
    sqlite3_update_hook(m_db->m_db, (void(*)(void*,int,const char*,const char*, wxsqlite_int64)) wxSQLite3FunctionContext::ExecUpdateHook, updateHook);
  }
  else
  {
    sqlite3_update_hook(m_db->m_db, (void(*)(void*,int,const char*,const char*, wxsqlite_int64)) NULL, NULL);
  }
}

void wxSQLite3Database::SetWriteAheadLogHook(wxSQLite3Hook* walHook)
{
#if SQLITE_VERSION_NUMBER >= 3007000
  CheckDatabase();
  if (walHook)
  {
    walHook->SetDatabase(this);
    sqlite3_wal_hook(m_db->m_db, (int(*)(void *,sqlite3*,const char*,int)) wxSQLite3FunctionContext::ExecWriteAheadLogHook, walHook);
  }
  else
  {
    sqlite3_wal_hook(m_db->m_db, (int(*)(void *,sqlite3*,const char*,int)) NULL, NULL);
  }
#else
  wxUnusedVar(walHook);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOWAL);
#endif
}

void wxSQLite3Database::WriteAheadLogCheckpoint(const wxString& database, int mode, 
                                                int* logFrameCount, int* ckptFrameCount)
{
#if SQLITE_VERSION_NUMBER >= 3007000
  CheckDatabase();
  wxCharBuffer strDatabase = database.ToUTF8();
  const char* localDatabase = strDatabase;
#if SQLITE_VERSION_NUMBER >= 3007006
  int rc = sqlite3_wal_checkpoint_v2(m_db->m_db, localDatabase, mode, logFrameCount, ckptFrameCount);
#else
  int rc = sqlite3_wal_checkpoint(m_db->m_db, localDatabase);
  if (logFrameCount  != NULL) *logFrameCount  = 0;
  if (ckptFrameCount != NULL) *ckptFrameCount = 0;
#endif

  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
#else
  wxUnusedVar(database);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOWAL);
#endif
}

void wxSQLite3Database::AutoWriteAheadLogCheckpoint(int frameCount)
{
#if SQLITE_VERSION_NUMBER >= 3007000
  CheckDatabase();
  int rc = sqlite3_wal_autocheckpoint(m_db->m_db, frameCount);

  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
#else
  wxUnusedVar(frameCount);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOWAL);
#endif
}

void wxSQLite3Database::SetCollation(const wxString& collationName, wxSQLite3Collation* collation)
{
  CheckDatabase();
  wxCharBuffer strCollationName = collationName.ToUTF8();
  const char* localCollationName = strCollationName;
  int rc;
  if (collation)
  {
    rc = sqlite3_create_collation(m_db->m_db, localCollationName, SQLITE_UTF8, collation, (int(*)(void*,int,const void*,int,const void*)) wxSQLite3Database::ExecComparisonWithCollation);
  }
  else
  {
    rc = sqlite3_create_collation(m_db->m_db, localCollationName, SQLITE_UTF8, NULL, (int(*)(void*,int,const void*,int,const void*)) NULL);
  }
}

void* wxSQLite3Database::GetDatabaseHandle() const
{
  return (m_db != NULL) ? m_db->m_db : NULL;
}

void wxSQLite3Database::SetCollationNeededCallback()
{
  CheckDatabase();
  int rc = sqlite3_collation_needed(m_db->m_db, this, (void(*)(void*,sqlite3*,int,const char*)) wxSQLite3Database::ExecCollationNeeded);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

void wxSQLite3Database::CheckDatabase() const
{
  if (m_db == NULL || m_db->m_db == NULL || !m_db->m_isValid || !m_isOpen)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NODB);
  }
}

void* wxSQLite3Database::Prepare(const char* sql)
{
  CheckDatabase();

  const char* tail=0;
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(m_db->m_db, sql, -1, &stmt, &tail);

  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  return stmt;
}

void* wxSQLite3Database::PreparePersistent(const char* sql)
{
#if SQLITE_VERSION_NUMBER >= 3020000
  CheckDatabase();

  const char* tail = 0;
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v3(m_db->m_db, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, &tail);

  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  return stmt;
#else
  return Prepare(sql);
#endif
}

/* static */
int wxSQLite3Database::ExecComparisonWithCollation(void* collation,
                                                   int len1, const void* text1,
                                                   int len2, const void* text2)
{
  wxString locText1 = wxString::FromUTF8((const char*) text1, (size_t) len1);
  wxString locText2 = wxString::FromUTF8((const char*) text2, (size_t) len2);
  return ((wxSQLite3Collation*) collation)->Compare(locText1, locText2);
}

void wxSQLite3Database::ExecCollationNeeded(void* db, void*, int, const char* collationName)
{
  wxString locCollation = wxString::FromUTF8((const char*) collationName);
  ((wxSQLite3Database*) db)->SetNeededCollation(locCollation);
}

void wxSQLite3Database::GetMetaData(const wxString& databaseName, const wxString& tableName, const wxString& columnName,
                                    wxString* dataType, wxString* collation, bool* notNull, bool* primaryKey, bool* autoIncrement)
{
#if SQLITE_ENABLE_COLUMN_METADATA
  CheckDatabase();
  wxCharBuffer strDatabaseName = databaseName.ToUTF8();
  const char* localDatabaseName = strDatabaseName;
  if (databaseName == wxEmptyString) localDatabaseName = NULL;
  wxCharBuffer strTableName = tableName.ToUTF8();
  const char* localTableName = strTableName;
  wxCharBuffer strColumnName = columnName.ToUTF8();
  const char* localColumnName = strColumnName;
  const char* localDataType;
  const char* localCollation;
  int localNotNull;
  int localPrimaryKey;
  int localAutoIncrement;
  int rc = sqlite3_table_column_metadata(m_db->m_db, localDatabaseName, localTableName, localColumnName,
                                         &localDataType, &localCollation, &localNotNull, &localPrimaryKey, &localAutoIncrement);

  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }

  if (dataType      != NULL) *dataType      = wxString::FromUTF8(localDataType);
  if (collation     != NULL) *collation     = wxString::FromUTF8(localCollation);

  if (notNull       != NULL) *notNull       = (localNotNull       != 0);
  if (primaryKey    != NULL) *primaryKey    = (localPrimaryKey    != 0);
  if (autoIncrement != NULL) *autoIncrement = (localAutoIncrement != 0);
#else
  wxUnusedVar(databaseName);
  wxUnusedVar(tableName);
  wxUnusedVar(columnName);
  wxUnusedVar(dataType);
  wxUnusedVar(collation);
  wxUnusedVar(notNull);
  wxUnusedVar(primaryKey);
  wxUnusedVar(autoIncrement);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOMETADATA);
#endif
}

void wxSQLite3Database::LoadExtension(const wxString& fileName, const wxString& entryPoint)
{
#if WXSQLITE3_HAVE_LOAD_EXTENSION
  CheckDatabase();
  wxCharBuffer strFileName = fileName.ToUTF8();
  const char* localFileName = strFileName;
  wxCharBuffer strEntryPoint = entryPoint.ToUTF8();
  const char* localEntryPoint = strEntryPoint;

  int rc = sqlite3_load_extension(m_db->m_db, localFileName, localEntryPoint, NULL);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
#else
  wxUnusedVar(fileName);
  wxUnusedVar(entryPoint);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOLOADEXT);
#endif
}

void wxSQLite3Database::EnableLoadExtension(bool enable)
{
#if WXSQLITE3_HAVE_LOAD_EXTENSION
  CheckDatabase();
  int onoff = (enable) ? 1 : 0;
  int rc = sqlite3_enable_load_extension(m_db->m_db, onoff);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
#else
  wxUnusedVar(enable);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOLOADEXT);
#endif
}

void wxSQLite3Database::ReKey(const wxString& newKey)
{
  ReKey(wxSQLite3Cipher(), newKey);
}

void wxSQLite3Database::ReKey(const wxSQLite3Cipher& cipher, const wxString& newKey)
{
  wxCharBuffer strLocalNewKey = newKey.ToUTF8();
  const char* localNewKey = strLocalNewKey;
  wxMemoryBuffer binaryNewKey;
  if (newKey.Length() > 0)
  {
    binaryNewKey.AppendData((void*) localNewKey, strlen(localNewKey));
  }
  ReKey(cipher, binaryNewKey);
}

void wxSQLite3Database::ReKey(const wxMemoryBuffer& newKey)
{
  ReKey(wxSQLite3Cipher(), newKey);
}

void wxSQLite3Database::ReKey(const wxSQLite3Cipher& cipher, const wxMemoryBuffer& newKey)
{
  CheckDatabase();
  if (cipher.IsOk())
  {
    if (!cipher.Apply(m_db->m_db))
    {
      throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_APPLY_FAILED);
    }
  }
  int rc = sqlite3_rekey(m_db->m_db, newKey.GetData(), (int) newKey.GetDataLen());
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

wxString wxSQLite3Database::GetKeySalt(const wxString& schemaName) const
{
  wxString keySalt = wxEmptyString;
  if (IsOpen())
  {
    const char* localSchemaName = NULL;
    wxCharBuffer strSchema = schemaName.ToUTF8();
    if (!schemaName.IsEmpty())
    {
      localSchemaName = strSchema;
    }
    char* localKeySalt = (char*) sqlite3mc_codec_data(m_db->m_db, localSchemaName, "cipher_salt");
    if (localKeySalt != NULL)
    {
      keySalt = wxString::FromUTF8(localKeySalt);
      sqlite3_free(localKeySalt);
    }
  }
  return keySalt;
}

bool wxSQLite3Database::UserLogin(const wxString& username, const wxString& password)
{
  wxUnusedVar(username);
  wxUnusedVar(password);
  return true;
}

bool wxSQLite3Database::UserAdd(const wxString& username, const wxString& password, bool isAdmin)
{
  wxUnusedVar(username);
  wxUnusedVar(password);
  wxUnusedVar(isAdmin);
  return true;
}

bool wxSQLite3Database::UserChange(const wxString& username, const wxString& password, bool isAdmin)
{
  wxUnusedVar(username);
  wxUnusedVar(password);
  wxUnusedVar(isAdmin);
  return false;
}

bool wxSQLite3Database::UserDelete(const wxString& username)
{
  wxUnusedVar(username);
  return false;
}

bool wxSQLite3Database::UserIsPrivileged(const wxString& username)
{
  wxUnusedVar(username);
  return false;
}

void wxSQLite3Database::GetUserList(wxArrayString& userList)
{
  userList.Empty();
}

int wxSQLite3Database::GetLimit(wxSQLite3LimitType id) const
{
  int value = -1;
  CheckDatabase();
  if (id >= WXSQLITE_LIMIT_LENGTH && id <= WXSQLITE_LIMIT_WORKER_THREADS)
  {
    value = sqlite3_limit(m_db->m_db, id, -1);
  }
  return value;
}

int wxSQLite3Database::SetLimit(wxSQLite3LimitType id, int newValue)
{
  int value = -1;
  CheckDatabase();
  if (id >= WXSQLITE_LIMIT_LENGTH && id <= WXSQLITE_LIMIT_WORKER_THREADS)
  {
    value = sqlite3_limit(m_db->m_db, id, newValue);
  }
  return value;
}

void wxSQLite3Database::ReleaseMemory()
{
  CheckDatabase();
  int rc = sqlite3_db_release_memory(m_db->m_db);
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
}

int wxSQLite3Database::GetSystemErrorCode() const
{
  int rc = 0;
  if (m_db != NULL)
  {
    rc = sqlite3_system_errno(m_db->m_db);
  }
  return rc;
}

#if wxCHECK_VERSION(2,9,0)
static const wxStringCharType* limitCodeString[] =
#else
static const wxChar* limitCodeString[] =
#endif
{ wxS("SQLITE_LIMIT_LENGTH"),              wxS("SQLITE_LIMIT_SQL_LENGTH"),
  wxS("SQLITE_LIMIT_COLUMN"),              wxS("SQLITE_LIMIT_EXPR_DEPTH"),
  wxS("SQLITE_LIMIT_COMPOUND_SELECT"),     wxS("SQLITE_LIMIT_VDBE_OP"),
  wxS("SQLITE_LIMIT_FUNCTION_ARG"),        wxS("SQLITE_LIMIT_ATTACHED"),
  wxS("SQLITE_LIMIT_LIKE_PATTERN_LENGTH"), wxS("SQLITE_LIMIT_VARIABLE_NUMBER"),
  wxS("SQLITE_LIMIT_TRIGGER_DEPTH"),       wxS("SQLITE_LIMIT_WORKER_THREADS")
};


/* static */
wxString wxSQLite3Database::LimitTypeToString(wxSQLite3LimitType type)
{
  wxString limitString(wxS("Unknown"));
  if (type >= WXSQLITE_LIMIT_LENGTH && type <= WXSQLITE_LIMIT_WORKER_THREADS)
  {
    limitString = limitCodeString[type];
  }
  return limitString;
}

/* static */
void wxSQLite3Database::InitializeSQLite()
{
  int rc = sqlite3_initialize();
  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_INITIALIZE);
  }
}

/* static */
void wxSQLite3Database::InitializeSQLite(const wxSQLite3Logger& logger)
{
  int rc = sqlite3_config(SQLITE_CONFIG_LOG, wxSQLite3Logger::ExecLoggerHook, &logger);
  if (rc == SQLITE_OK)
  {
    InitializeSQLite();
  }
  else
  {
    throw wxSQLite3Exception(rc, wxERRMSG_INITIALIZE);
  }
}

/* static */
void wxSQLite3Database::ShutdownSQLite()
{
  int rc = sqlite3_shutdown();
  if (rc != SQLITE_OK)
  {
    throw wxSQLite3Exception(rc, wxERRMSG_SHUTDOWN);
  }
}

/* static */
bool wxSQLite3Database::SetTemporaryDirectory(const wxString& tempDirectory)
{
  bool ok = false;
#if defined(__WXMSW__)
#if wxUSE_UNICODE
  const wxChar* zValue = tempDirectory.wc_str();
#else
  const wxWCharBuffer zValue = tempDirectory.wc_str(wxConvLocal);
#endif
  int rc = sqlite3_win32_set_directory(SQLITE_WIN32_TEMP_DIRECTORY_TYPE, (void*) zValue);
  ok = (rc == SQLITE_OK);
#endif
  return ok;
}

/* static */
bool wxSQLite3Database::Randomness(int n, wxMemoryBuffer& random)
{
  bool ok = false;
  if (n > 0)
  {
    void* buffer = random.GetWriteBuf(n);
    sqlite3_randomness(n, buffer);
    random.UngetWriteBuf(n);
    ok = true;
  }
  return ok;
}

// ----------------------------------------------------------------------------
// wxSQLite3FunctionContext: class providing the function context
//                           for user defined functions
// ----------------------------------------------------------------------------

int wxSQLite3FunctionContext::GetArgCount() const
{
  return m_argc;
}

int wxSQLite3FunctionContext::GetArgType(int argIndex) const
{
  if (argIndex >= 0 && argIndex < m_argc)
  {
    return sqlite3_value_type((sqlite3_value*) m_argv[argIndex]);
  }
  else
  {
    return SQLITE_NULL;
  }
}

bool wxSQLite3FunctionContext::IsNull(int argIndex) const
{
  if (argIndex >= 0 && argIndex < m_argc)
  {
    return sqlite3_value_type((sqlite3_value*) m_argv[argIndex]) == SQLITE_NULL;
  }
  else
  {
    return true;
  }
}

int wxSQLite3FunctionContext::GetInt(int argIndex, int nullValue) const
{
  if (argIndex >= 0 && argIndex < m_argc)
  {
    if (!IsNull(argIndex))
    {
      return sqlite3_value_int((sqlite3_value*) m_argv[argIndex]);
    }
    else
    {
      return nullValue;
    }
  }
  else
  {
    return nullValue;
  }
}

wxLongLong wxSQLite3FunctionContext::GetInt64(int argIndex, wxLongLong nullValue) const
{
  if (argIndex >= 0 && argIndex < m_argc)
  {
    if (!IsNull(argIndex))
    {
      return wxLongLong(sqlite3_value_int64((sqlite3_value*) m_argv[argIndex]));
    }
    else
    {
      return nullValue;
    }
  }
  else
  {
    return nullValue;
  }
}

double wxSQLite3FunctionContext::GetDouble(int argIndex, double nullValue) const
{
  if (argIndex >= 0 && argIndex < m_argc)
  {
    if (!IsNull(argIndex))
    {
      return sqlite3_value_double((sqlite3_value*) m_argv[argIndex]);
    }
    else
    {
      return nullValue;
    }
  }
  else
  {
    return nullValue;
  }
}

wxString wxSQLite3FunctionContext::GetString(int argIndex, const wxString& nullValue) const
{
  if (argIndex >= 0 && argIndex < m_argc)
  {
    if (!IsNull(argIndex))
    {
      const char* localValue = (const char*) sqlite3_value_text((sqlite3_value*) m_argv[argIndex]);
      return wxString::FromUTF8(localValue);
    }
    else
    {
      return nullValue;
    }
  }
  else
  {
    return nullValue;
  }
}

const unsigned char* wxSQLite3FunctionContext::GetBlob(int argIndex, int& len) const
{
  const unsigned char* buffer = NULL;
  if (argIndex >= 0 && argIndex < m_argc)
  {
    if (!IsNull(argIndex))
    {
      len = sqlite3_value_bytes((sqlite3_value*) m_argv[argIndex]);
      buffer = (const unsigned char*) sqlite3_value_blob((sqlite3_value*) m_argv[argIndex]);
    }
  }
  return buffer;
}

wxMemoryBuffer& wxSQLite3FunctionContext::GetBlob(int argIndex, wxMemoryBuffer& buffer) const
{
  if (argIndex >= 0 && argIndex < m_argc)
  {
    if (!IsNull(argIndex))
    {
      int len = sqlite3_value_bytes((sqlite3_value*) m_argv[argIndex]);
      const void* blob = sqlite3_value_blob((sqlite3_value*) m_argv[argIndex]);
      buffer.AppendData((void*) blob, (size_t) len);
    }
  }
  return buffer;
}

void* wxSQLite3FunctionContext::GetPointer(int argIndex, const wxString& pointerType) const
{
  void* pointer = NULL;
#if SQLITE_VERSION_NUMBER >= 3020000
  if (argIndex >= 0 && argIndex < m_argc)
  {
    if (!IsNull(argIndex))
    {
      wxCharBuffer strPointerType = pointerType.ToUTF8();
      const char* localPointerType = strPointerType;

      pointer = sqlite3_value_pointer((sqlite3_value*) m_argv[argIndex], localPointerType);
    }
  }
#endif
  return pointer;
}

void wxSQLite3FunctionContext::SetResult(int value)
{
  sqlite3_result_int((sqlite3_context*) m_ctx, value);
}

void wxSQLite3FunctionContext::SetResult(wxLongLong value)
{
  sqlite3_result_int64((sqlite3_context*) m_ctx, value.GetValue());
}

void wxSQLite3FunctionContext::SetResult(double value)
{
  sqlite3_result_double((sqlite3_context*) m_ctx, value);
}

void wxSQLite3FunctionContext::SetResult(const wxString& value)
{
  wxCharBuffer strValue = value.ToUTF8();
  const char* localValue = strValue;
  sqlite3_result_text((sqlite3_context*) m_ctx, localValue, -1, SQLITE_TRANSIENT);
}

void wxSQLite3FunctionContext::SetResult(unsigned char* value, int len)
{
  sqlite3_result_blob((sqlite3_context*) m_ctx, value, len, SQLITE_TRANSIENT);
}

void wxSQLite3FunctionContext::SetResult(const wxMemoryBuffer& buffer)
{
  sqlite3_result_blob((sqlite3_context*) m_ctx, buffer.GetData(), (int) buffer.GetDataLen(), SQLITE_TRANSIENT);
}

void wxSQLite3FunctionContext::SetResult(void* pointer, const wxString& pointerType, void(*DeletePointer)(void*))
{
#if SQLITE_VERSION_NUMBER >= 3020000
  const char* localPointerType = MakePointerTypeCopy(pointerType);
  sqlite3_result_pointer((sqlite3_context*) m_ctx, pointer, localPointerType, DeletePointer);
#endif
}

void wxSQLite3FunctionContext::SetResultNull()
{
  sqlite3_result_null((sqlite3_context*) m_ctx);
}

void wxSQLite3FunctionContext::SetResultZeroBlob(int blobSize)
{
#if SQLITE_VERSION_NUMBER >= 3004000
  sqlite3_result_zeroblob((sqlite3_context*) m_ctx, blobSize);
#endif
}

void wxSQLite3FunctionContext::SetResultArg(int argIndex)
{
  if (argIndex >= 0 && argIndex < m_argc) {
    sqlite3_result_value((sqlite3_context*) m_ctx, (sqlite3_value*) m_argv[argIndex]);
  } else {
    sqlite3_result_null((sqlite3_context*) m_ctx);
  }
}

void wxSQLite3FunctionContext::SetResultError(const wxString& errmsg)
{
  wxCharBuffer strErrmsg = errmsg.ToUTF8();
  const char* localErrmsg = strErrmsg;
  sqlite3_result_error((sqlite3_context*) m_ctx, localErrmsg, -1);
}

int wxSQLite3FunctionContext::GetAggregateCount() const
{
  if (m_isAggregate)
  {
    return m_count;
  }
  else
  {
    return 0;
  }
}

void* wxSQLite3FunctionContext::GetAggregateStruct(int len) const
{
  if (m_isAggregate)
  {
    return sqlite3_aggregate_context((sqlite3_context*) m_ctx, len);
  }
  else
  {
    return NULL;
  }
}

/* static */
void wxSQLite3FunctionContext::ExecScalarFunction(void* ctx, int argc, void** argv)
{
  wxSQLite3FunctionContext context(ctx, false, argc, argv);
  wxSQLite3ScalarFunction* func = (wxSQLite3ScalarFunction*) sqlite3_user_data((sqlite3_context*) ctx);
  func->Execute(context);
}

/* static */
void wxSQLite3FunctionContext::ExecAggregateStep(void* ctx, int argc, void** argv)
{
  wxSQLite3FunctionContext context(ctx, true, argc, argv);
  wxSQLite3AggregateFunction* func = (wxSQLite3AggregateFunction*) sqlite3_user_data((sqlite3_context*) ctx);
  func->m_count++;
  context.m_count = func->m_count;
  func->Aggregate(context);
}

/* static */
void wxSQLite3FunctionContext::ExecAggregateFinalize(void* ctx)
{
  wxSQLite3FunctionContext context(ctx, true, 0, NULL);
  wxSQLite3AggregateFunction* func = (wxSQLite3AggregateFunction*) sqlite3_user_data((sqlite3_context*) ctx);
  context.m_count = func->m_count;
  func->Finalize(context);
}

/* static */
void wxSQLite3FunctionContext::ExecWindowStep(void* ctx, int argc, void** argv)
{
  wxSQLite3FunctionContext context(ctx, true, argc, argv);
  wxSQLite3WindowFunction* func = (wxSQLite3WindowFunction*) sqlite3_user_data((sqlite3_context*) ctx);
  func->m_count++;
  context.m_count = func->m_count;
  func->Aggregate(context);
}

/* static */
void wxSQLite3FunctionContext::ExecWindowFinalize(void* ctx)
{
  wxSQLite3FunctionContext context(ctx, true, 0, NULL);
  wxSQLite3WindowFunction* func = (wxSQLite3WindowFunction*) sqlite3_user_data((sqlite3_context*) ctx);
  context.m_count = func->m_count;
  func->Finalize(context);
}

/* static */
void wxSQLite3FunctionContext::ExecWindowValue(void* ctx)
{
  wxSQLite3FunctionContext context(ctx, true, 0, NULL);
  wxSQLite3WindowFunction* func = (wxSQLite3WindowFunction*) sqlite3_user_data((sqlite3_context*) ctx);
  context.m_count = func->m_count;
  func->CurrentValue(context);
}

/* static */
void wxSQLite3FunctionContext::ExecWindowInverse(void* ctx, int argc, void** argv)
{
  wxSQLite3FunctionContext context(ctx, true, argc, argv);
  wxSQLite3WindowFunction* func = (wxSQLite3WindowFunction*) sqlite3_user_data((sqlite3_context*) ctx);
  func->m_count--;
  context.m_count = func->m_count;
  func->Reverse(context);
}

// 
static int wxSQLite3FunctionContextExecAuthorizer(void* func, int type,
                                           const char* arg1, const char* arg2,
                                           const char* arg3, const char* arg4
                                          )
{
  wxString locArg1 = wxString::FromUTF8(arg1);
  wxString locArg2 = wxString::FromUTF8(arg2);
  wxString locArg3 = wxString::FromUTF8(arg3);
  wxString locArg4 = wxString::FromUTF8(arg4);
  wxString locArg5 = wxEmptyString;
  wxSQLite3Authorizer::wxAuthorizationCode localType = (wxSQLite3Authorizer::wxAuthorizationCode) type;
  return (int) ((wxSQLite3Authorizer*) func)->Authorize(localType, locArg1, locArg2, locArg3, locArg4, locArg5);
}

/* static */
int wxSQLite3FunctionContext::ExecCommitHook(void* hook)
{
  return (int) ((wxSQLite3Hook*) hook)->CommitCallback();
}

/* static */
void wxSQLite3FunctionContext::ExecRollbackHook(void* hook)
{
  ((wxSQLite3Hook*) hook)->RollbackCallback();
}

/* static */
void wxSQLite3FunctionContext::ExecUpdateHook(void* hook, int type,
                                              const char* database, const char* table,
                                              wxsqlite_int64 rowid)
{
  wxString locDatabase = wxString::FromUTF8(database);
  wxString locTable = wxString::FromUTF8(table);
  wxSQLite3Hook::wxUpdateType locType = (wxSQLite3Hook::wxUpdateType) type;
  wxLongLong locRowid = rowid;
  ((wxSQLite3Hook*) hook)->UpdateCallback(locType, locDatabase, locTable, locRowid);
}

wxSQLite3FunctionContext::wxSQLite3FunctionContext(void* ctx, bool isAggregate, int argc, void** argv)
  : m_ctx(ctx), m_isAggregate(isAggregate), m_count(0), m_argc(argc), m_argv(argv), m_ptrTypes(NULL)
{
}

wxSQLite3FunctionContext::~wxSQLite3FunctionContext()
{
  if (m_ptrTypes != NULL)
  {
    size_t n = m_ptrTypes->GetCount();
    for (size_t j = 0; j < n; ++j)
    {
      sqlite3_free((*m_ptrTypes)[j]);
    }
    delete m_ptrTypes;
  }
}

const char* wxSQLite3FunctionContext::MakePointerTypeCopy(const wxString& pointerType)
{
  // Allocate pointer type array if necessary
  if (m_ptrTypes == NULL)
  {
    m_ptrTypes = new wxArrayPtrVoid();
  }

  // Convert pointer type to char*
  return LocalMakePointerTypeCopy(*m_ptrTypes, pointerType);
}

/* static */
int wxSQLite3FunctionContext::ExecWriteAheadLogHook(void* hook, void* dbHandle,
                                                    const char* database, int numPages)
{
  wxString locDatabase = wxString::FromUTF8(database);
  wxUnusedVar(dbHandle);
  return (int) ((wxSQLite3Hook*) hook)->WriteAheadLogCallback(locDatabase, numPages);
}

#if wxCHECK_VERSION(2,9,0)
static const wxStringCharType* authCodeString[] =
#else
static const wxChar* authCodeString[] =
#endif
{ wxS("SQLITE_COPY"),              wxS("SQLITE_CREATE_INDEX"),      wxS("SQLITE_CREATE_TABLE"),
  wxS("SQLITE_CREATE_TEMP_INDEX"), wxS("SQLITE_CREATE_TEMP_TABLE"), wxS("SQLITE_CREATE_TEMP_TRIGGER"),
  wxS("SQLITE_CREATE_TEMP_VIEW"),  wxS("SQLITE_CREATE_TRIGGER"),    wxS("SQLITE_CREATE_VIEW"),
  wxS("SQLITE_DELETE"),            wxS("SQLITE_DROP_INDEX"),        wxS("SQLITE_DROP_TABLE"),
  wxS("SQLITE_DROP_TEMP_INDEX"),   wxS("SQLITE_DROP_TEMP_TABLE"),   wxS("SQLITE_DROP_TEMP_TRIGGER"),
  wxS("SQLITE_DROP_TEMP_VIEW"),    wxS("SQLITE_DROP_TRIGGER"),      wxS("SQLITE_DROP_VIEW"),
  wxS("SQLITE_INSERT"),            wxS("SQLITE_PRAGMA"),            wxS("SQLITE_READ"),
  wxS("SQLITE_SELECT"),            wxS("SQLITE_TRANSACTION"),       wxS("SQLITE_UPDATE"),
  wxS("SQLITE_ATTACH"),            wxS("SQLITE_DETACH"),            wxS("SQLITE_ALTER_TABLE"),
  wxS("SQLITE_REINDEX"),           wxS("SQLITE_ANALYZE"),           wxS("SQLITE_CREATE_VTABLE"),
  wxS("SQLITE_DROP_VTABLE"),       wxS("SQLITE_FUNCTION"),          wxS("SQLITE_SAVEPOINT"),
  wxS("SQLITE_RECURSIVE")
};


/* static */
wxString wxSQLite3Authorizer::AuthorizationCodeToString(wxSQLite3Authorizer::wxAuthorizationCode type)
{
  wxString authString(wxS("Unknown"));
  if (type >= SQLITE_COPY && type <= SQLITE_MAX_CODE)
  {
    authString = authCodeString[type];
  }
  return authString;
}

// ----------------------------------------------------------------------------
// wxSQLite3Transaction
// ----------------------------------------------------------------------------

wxSQLite3Transaction::wxSQLite3Transaction(wxSQLite3Database* db, wxSQLite3TransactionType transactionType)
{
  wxASSERT(db != NULL);
  m_database = db;
  try
  {
    m_database->Begin(transactionType);
  }
  catch (...)
  {
    m_database = NULL; // Flag that transaction is not active
  }
}

wxSQLite3Transaction::~wxSQLite3Transaction()
{
  if (m_database != NULL)
  {
    try
    {
      m_database->Rollback();
    }
    catch (...)
    {
      // Intentionally do nothing
    }
  }
}

void wxSQLite3Transaction::Commit()
{
  m_database->Commit();
  m_database = NULL;
}

void wxSQLite3Transaction::Rollback()
{
  m_database->Rollback();
  m_database = NULL;
}

// --- SQLite logging

wxSQLite3Logger::wxSQLite3Logger()
  : m_isActive(false)
{
}

wxSQLite3Logger::~wxSQLite3Logger()
{
}

void
wxSQLite3Logger::HandleLogMessage(int errorCode, const wxString& errorMessage)
{
  if (m_isActive)
  {
#if wxCHECK_VERSION(2,9,0)
    wxLogInfo(wxS("SQLite3 %s (%d): %s"), wxSQLite3Exception::ErrorCodeAsString(errorCode), errorCode, errorMessage);
#else
    wxLogInfo(wxS("SQLite3 %s (%d): %s"), wxSQLite3Exception::ErrorCodeAsString(errorCode).c_str(), errorCode, errorMessage.c_str());
#endif
  }
}

/* static */
void
wxSQLite3Logger::ExecLoggerHook(void* logger, int errorCode, const char* errorMsg)
{
  ((wxSQLite3Logger*) logger)->HandleLogMessage(errorCode, wxString::FromUTF8(errorMsg));
}

// --- User defined function classes

#if wxUSE_REGEX

wxSQLite3RegExpOperator::wxSQLite3RegExpOperator(int flags) : m_flags(flags)
{
}

wxSQLite3RegExpOperator::~wxSQLite3RegExpOperator()
{
}

void wxSQLite3RegExpOperator::Execute(wxSQLite3FunctionContext& ctx)
{
  int argCount = ctx.GetArgCount();
  if (argCount == 2)
  {
    wxString exprStr = ctx.GetString(0);
    wxString textStr = ctx.GetString(1);
    if (!m_exprStr.IsSameAs(exprStr))
    {
      m_exprStr = exprStr;
      m_regEx.Compile(m_exprStr, m_flags);
    }
    if (m_regEx.IsValid())
    {
      int rc = (m_regEx.Matches(textStr)) ? 1 : 0;
      ctx.SetResult(rc);
    }
    else
    {
      ctx.SetResultError(wxString(_("Regular expression invalid: '"))+exprStr+_T("'."));
    }
  }
  else
  {
    ctx.SetResultError(wxString::Format(_("REGEXP called with wrong number of arguments: %d instead of 2."), argCount));
  }
}

#endif

// --- Support for named collections

#if WXSQLITE3_USE_NAMED_COLLECTIONS

// The following code is based on the SQLite test_intarray source code.

#include <string.h>
#include <assert.h>

/// Definition of the sqlite3_intarray object (internal)
struct sqlite3_intarray
{
  int n;                    // Number of elements in the array
  sqlite3_int64* a;         // Contents of the array
  void (*xFree)(void*);     // Function used to free a[]
};

// Objects used internally by the virtual table implementation
typedef struct intarray_vtab intarray_vtab;
typedef struct intarray_cursor intarray_cursor;

/// Definition of intarray table object (internal)
struct intarray_vtab
{
  sqlite3_vtab base;            // Base class
  sqlite3_intarray* pContent;   // Content of the integer array
};

/// Definition of  intarray cursor object (internal)
struct intarray_cursor
{
  sqlite3_vtab_cursor base;    // Base class
  int i;                       // Current cursor position
};

// Free an sqlite3_intarray object.
static void intarrayFree(sqlite3_intarray* p)
{
  if (p->a != NULL && p->xFree)
  {
    p->xFree(p->a);
  }
  sqlite3_free(p);
}

// Table destructor for the intarray module.
static int intarrayDestroy(sqlite3_vtab* p)
{
  intarray_vtab* pVtab = (intarray_vtab*)p;
  sqlite3_free(pVtab);
  return 0;
}

// Table constructor for the intarray module.
static int intarrayCreate(sqlite3* db,                  // Database where module is created
                          void* pAux,                   // clientdata for the module
                          int /*argc*/,                 // Number of arguments
                          const char* const* /*argv*/,  // Value for all arguments
                          sqlite3_vtab** ppVtab,        // Write the new virtual table object here
                          char** /*pzErr*/)             // Put error message text here
{
  int rc = SQLITE_NOMEM;
  intarray_vtab* pVtab = (intarray_vtab*) sqlite3_malloc(sizeof(intarray_vtab));

  if (pVtab)
  {
    memset(pVtab, 0, sizeof(intarray_vtab));
    pVtab->pContent = (sqlite3_intarray*)pAux;
    rc = sqlite3_declare_vtab(db, "CREATE TABLE x(value INTEGER PRIMARY KEY)");
  }
  *ppVtab = (sqlite3_vtab*)pVtab;
  return rc;
}

// Open a new cursor on the intarray table.
static int intarrayOpen(sqlite3_vtab* /*pVTab*/, sqlite3_vtab_cursor** ppCursor)
{
  int rc = SQLITE_NOMEM;
  intarray_cursor* pCur = (intarray_cursor*) sqlite3_malloc(sizeof(intarray_cursor));
  if (pCur)
  {
    memset(pCur, 0, sizeof(intarray_cursor));
    *ppCursor = (sqlite3_vtab_cursor *)pCur;
    rc = SQLITE_OK;
  }
  return rc;
}

// Close a intarray table cursor.
static int intarrayClose(sqlite3_vtab_cursor* cur)
{
  intarray_cursor* pCur = (intarray_cursor*)cur;
  sqlite3_free(pCur);
  return SQLITE_OK;
}

// Retrieve a column of data.
static int intarrayColumn(sqlite3_vtab_cursor* cur, sqlite3_context* ctx, int /*i*/)
{
  intarray_cursor* pCur = (intarray_cursor*)cur;
  intarray_vtab* pVtab = (intarray_vtab*)cur->pVtab;
  if (pCur->i >= 0 && pCur->i < pVtab->pContent->n)
  {
    sqlite3_result_int64(ctx, pVtab->pContent->a[pCur->i]);
  }
  return SQLITE_OK;
}

// Retrieve the current rowid.
static int intarrayRowid(sqlite3_vtab_cursor* cur, sqlite_int64* pRowid)
{
  intarray_cursor* pCur = (intarray_cursor*)cur;
  *pRowid = pCur->i;
  return SQLITE_OK;
}

static int intarrayEof(sqlite3_vtab_cursor* cur)
{
  intarray_cursor* pCur = (intarray_cursor*)cur;
  intarray_vtab* pVtab = (intarray_vtab*)cur->pVtab;
  return pCur->i >= pVtab->pContent->n;
}

// Advance the cursor to the next row.
static int intarrayNext(sqlite3_vtab_cursor* cur)
{
  intarray_cursor* pCur = (intarray_cursor*)cur;
  pCur->i++;
  return SQLITE_OK;
}

// Reset a intarray table cursor.
static int intarrayFilter(sqlite3_vtab_cursor* pVtabCursor,
                          int /*idxNum*/, const char* /*idxStr*/,
                          int /*argc*/, sqlite3_value** /*argv*/)
{
  intarray_cursor* pCur = (intarray_cursor*) pVtabCursor;
  pCur->i = 0;
  return SQLITE_OK;
}

// Analyse the WHERE condition.
static int intarrayBestIndex(sqlite3_vtab* /*tab*/, sqlite3_index_info* /*pIdxInfo*/)
{
  return SQLITE_OK;
}

// Definition of a virtual table module for integer collections
static sqlite3_module intarrayModule =
{
  0,                           // iVersion
  intarrayCreate,              // xCreate - create a new virtual table
  intarrayCreate,              // xConnect - connect to an existing vtab
  intarrayBestIndex,           // xBestIndex - find the best query index
  intarrayDestroy,             // xDisconnect - disconnect a vtab
  intarrayDestroy,             // xDestroy - destroy a vtab
  intarrayOpen,                // xOpen - open a cursor
  intarrayClose,               // xClose - close a cursor
  intarrayFilter,              // xFilter - configure scan constraints
  intarrayNext,                // xNext - advance a cursor
  intarrayEof,                 // xEof
  intarrayColumn,              // xColumn - read data
  intarrayRowid,               // xRowid - read data
  0,                           // xUpdate
  0,                           // xBegin
  0,                           // xSync
  0,                           // xCommit
  0,                           // xRollback
  0,                           // xFindMethod
  0,                           // xRename
#if SQLITE_VERSION_NUMBER >= 3007007
  0,                           // xSavepoint
  0,                           // xRelease
  0                            // xRollbackTo
#endif
};

/// Definition of the sqlite3_chararray object (internal)
struct sqlite3_chararray
{
  int n;                    // Number of elements in the array
  char** a;                 // Contents of the array
  void (*xFree)(void*);     // Function used to free a[]
};

// Objects used internally by the virtual table implementation
typedef struct chararray_vtab chararray_vtab;
typedef struct chararray_cursor chararray_cursor;

/// Definition of chararray table object (internal)
struct chararray_vtab
{
  sqlite3_vtab base;            // Base class
  sqlite3_chararray* pContent;  // Content of the char array
};

/// Definition of chararray cursor object (internal)
struct chararray_cursor
{
  sqlite3_vtab_cursor base;    // Base class
  int i;                       // Current cursor position
};

// Free an sqlite3_chararray object.
static void chararrayFree(sqlite3_chararray* p)
{
  if (p->a != NULL && p->xFree)
  {
    int j;
    for (j = 0; j < p->n; ++j)
    {
      p->xFree(p->a[j]);
    }
    p->xFree(p->a);
  }
  sqlite3_free(p);
}

// Table destructor for the chararray module.
static int chararrayDestroy(sqlite3_vtab* p)
{
  chararray_vtab* pVtab = (chararray_vtab*)p;
  sqlite3_free(pVtab);
  return 0;
}

// Table constructor for the chararray module.
static int chararrayCreate(sqlite3* db,                  // Database where module is created
                           void* pAux,                   // clientdata for the module
                           int /*argc*/,                 // Number of arguments
                           const char* const* /*argv*/,  // Value for all arguments
                           sqlite3_vtab** ppVtab,        // Write the new virtual table object here
                           char** /*pzErr*/)             // Put error message text here
{
  int rc = SQLITE_NOMEM;
  chararray_vtab* pVtab = (chararray_vtab*) sqlite3_malloc(sizeof(chararray_vtab));

  if (pVtab)
  {
    memset(pVtab, 0, sizeof(chararray_vtab));
    pVtab->pContent = (sqlite3_chararray*) pAux;
    rc = sqlite3_declare_vtab(db, "CREATE TABLE x(value CHAR PRIMARY KEY)");
  }
  *ppVtab = (sqlite3_vtab*) pVtab;
  return rc;
}

// Open a new cursor on the chararray table.
static int chararrayOpen(sqlite3_vtab* /*pVTab*/, sqlite3_vtab_cursor** ppCursor)
{
  int rc = SQLITE_NOMEM;
  chararray_cursor* pCur = (chararray_cursor*) sqlite3_malloc(sizeof(chararray_cursor));
  if (pCur)
  {
    memset(pCur, 0, sizeof(chararray_cursor));
    *ppCursor = (sqlite3_vtab_cursor *)pCur;
    rc = SQLITE_OK;
  }
  return rc;
}

// Close a chararray table cursor.
static int chararrayClose(sqlite3_vtab_cursor* cur)
{
  chararray_cursor* pCur = (chararray_cursor*)cur;
  sqlite3_free(pCur);
  return SQLITE_OK;
}

// Retrieve a column of data.
static int chararrayColumn(sqlite3_vtab_cursor* cur, sqlite3_context* ctx, int /*i*/)
{
  chararray_cursor* pCur = (chararray_cursor*)cur;
  chararray_vtab* pVtab = (chararray_vtab*)cur->pVtab;
  if (pCur->i >= 0 && pCur->i < pVtab->pContent->n)
  {
    sqlite3_result_text(ctx, pVtab->pContent->a[pCur->i], -1, SQLITE_STATIC);
  }
  return SQLITE_OK;
}

// Retrieve the current rowid.
static int chararrayRowid(sqlite3_vtab_cursor* cur, sqlite_int64* pRowid)
{
  chararray_cursor* pCur = (chararray_cursor*)cur;
  *pRowid = pCur->i;
  return SQLITE_OK;
}

static int chararrayEof(sqlite3_vtab_cursor* cur)
{
  chararray_cursor* pCur = (chararray_cursor*)cur;
  chararray_vtab* pVtab = (chararray_vtab*)cur->pVtab;
  return pCur->i >= pVtab->pContent->n;
}

// Advance the cursor to the next row.
static int chararrayNext(sqlite3_vtab_cursor* cur)
{
  chararray_cursor* pCur = (chararray_cursor*)cur;
  pCur->i++;
  return SQLITE_OK;
}

// Reset a chararray table cursor.
static int chararrayFilter(sqlite3_vtab_cursor* pVtabCursor,
                           int /*idxNum*/, const char* /*idxStr*/,
                           int /*argc*/, sqlite3_value** /*argv*/)
{
  chararray_cursor *pCur = (chararray_cursor *)pVtabCursor;
  pCur->i = 0;
  return SQLITE_OK;
}

// Analyse the WHERE condition.
static int chararrayBestIndex(sqlite3_vtab* /*tab*/, sqlite3_index_info* /*pIdxInfo*/)
{
  return SQLITE_OK;
}

// Definition of a virtual table module for string collections
static sqlite3_module chararrayModule =
{
  0,                           // iVersion
  chararrayCreate,             // xCreate - create a new virtual table
  chararrayCreate,             // xConnect - connect to an existing vtab
  chararrayBestIndex,          // xBestIndex - find the best query index
  chararrayDestroy,            // xDisconnect - disconnect a vtab
  chararrayDestroy,            // xDestroy - destroy a vtab
  chararrayOpen,               // xOpen - open a cursor
  chararrayClose,              // xClose - close a cursor
  chararrayFilter,             // xFilter - configure scan constraints
  chararrayNext,               // xNext - advance a cursor
  chararrayEof,                // xEof
  chararrayColumn,             // xColumn - read data
  chararrayRowid,              // xRowid - read data
  0,                           // xUpdate
  0,                           // xBegin
  0,                           // xSync
  0,                           // xCommit
  0,                           // xRollback
  0,                           // xFindMethod
  0,                           // xRename
  0,                           // xSavepoint
  0,                           // xRelease
  0                            // xRollbackTo
};

#endif // WXSQLITE3_USE_NAMED_COLLECTIONS

wxSQLite3NamedCollection::wxSQLite3NamedCollection(const wxString& collectionName, void* collectionData)
  : m_name(collectionName), m_data(collectionData)
{
}

wxSQLite3NamedCollection::wxSQLite3NamedCollection(const wxSQLite3NamedCollection& collection)
  : m_name(collection.m_name), m_data(collection.m_data)
{
}

wxSQLite3NamedCollection&
wxSQLite3NamedCollection::operator=(const wxSQLite3NamedCollection& collection)
{
  if (this != &collection)
  {
    m_name = collection.m_name;
    m_data = collection.m_data;
  }
  return *this;
}

wxSQLite3NamedCollection::~wxSQLite3NamedCollection()
{
}

wxSQLite3IntegerCollection::wxSQLite3IntegerCollection(const wxSQLite3IntegerCollection& collection)
  : wxSQLite3NamedCollection(collection)
{
}

wxSQLite3IntegerCollection&
wxSQLite3IntegerCollection::operator=(const wxSQLite3IntegerCollection& collection)
{
  if (this != &collection)
  {
    wxSQLite3NamedCollection::operator=(collection);
  }
  return *this;
}

wxSQLite3IntegerCollection::wxSQLite3IntegerCollection(const wxString& collectionName, void* collectionData)
  : wxSQLite3NamedCollection(collectionName, collectionData)
{
}

wxSQLite3IntegerCollection::~wxSQLite3IntegerCollection()
{
}

void
wxSQLite3IntegerCollection::Bind(const wxArrayInt& integerCollection)
{
  if (IsOk())
  {
    size_t n = integerCollection.Count();
    sqlite3_intarray* pIntArray = (sqlite3_intarray*) m_data;
    if (pIntArray->a != NULL && pIntArray->xFree)
    {
      pIntArray->xFree(pIntArray->a);
    }
    pIntArray->n = n;
    if (n > 0)
    {
      pIntArray->a = (sqlite3_int64*) sqlite3_malloc(sizeof(sqlite3_int64)*n);
      pIntArray->xFree = sqlite3_free;
    }
    else
    {
      pIntArray->a = NULL;
      pIntArray->xFree = NULL;
    }

    size_t j;
    for (j = 0; j < n; ++j)
    {
      pIntArray->a[j] = integerCollection[j];
    }
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_COLLECTION);
  }
}

void
wxSQLite3IntegerCollection::Bind(int n, int* integerCollection)
{
  if (IsOk())
  {
    sqlite3_intarray* pIntArray = (sqlite3_intarray*) m_data;
    if (pIntArray->a != NULL && pIntArray->xFree)
    {
      pIntArray->xFree(pIntArray->a);
    }
    pIntArray->n = n;
    if (n > 0)
    {
      pIntArray->a = (sqlite3_int64*) sqlite3_malloc(sizeof(sqlite3_int64)*n);
      pIntArray->xFree = sqlite3_free;
    }
    else
    {
      pIntArray->a = NULL;
      pIntArray->xFree = NULL;
    }

    int j;
    for (j = 0; j < n; ++j)
    {
      pIntArray->a[j] = integerCollection[j];
    }
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_COLLECTION);
  }
}

wxSQLite3IntegerCollection
wxSQLite3Database::CreateIntegerCollection(const wxString& collectionName)
{
#if WXSQLITE3_USE_NAMED_COLLECTIONS
  CheckDatabase();
  int rc = SQLITE_OK;
  wxCharBuffer strCollectionName = collectionName.ToUTF8();
  const char* zName = strCollectionName;
  sqlite3_intarray* p = (sqlite3_intarray*) sqlite3_malloc( sizeof(*p) );
  if (p == 0)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOMEM);
  }
  p->n = 0;
  p->a= NULL;
  p->xFree = NULL;
  rc = sqlite3_create_module_v2(m_db->m_db, zName, &intarrayModule, p, (void(*)(void*))intarrayFree);
  if (rc == SQLITE_OK)
  {
    wxSQLite3StatementBuffer zBuffer;
    const char* zSql = zBuffer.Format("CREATE VIRTUAL TABLE temp.\"%w\" USING \"%w\"", zName, zName);
    rc = sqlite3_exec(m_db->m_db, zSql, 0, 0, 0);
  }
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
  return wxSQLite3IntegerCollection(collectionName, p);
#else
  wxUnusedVar(collectionName);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOCOLLECTIONS);
#endif // WXSQLITE3_USE_NAMED_COLLECTIONS
}

wxSQLite3StringCollection::wxSQLite3StringCollection(const wxSQLite3StringCollection& collection)
  : wxSQLite3NamedCollection(collection)
{
}

wxSQLite3StringCollection&
wxSQLite3StringCollection::operator=(const wxSQLite3StringCollection& collection)
{
  if (this != &collection)
  {
    wxSQLite3NamedCollection::operator=(collection);
  }
  return *this;
}

wxSQLite3StringCollection::wxSQLite3StringCollection(const wxString& collectionName, void* collectionData)
  : wxSQLite3NamedCollection(collectionName, collectionData)
{
}

wxSQLite3StringCollection::~wxSQLite3StringCollection()
{
}

void
wxSQLite3StringCollection::Bind(const wxArrayString& stringCollection)
{
  if (IsOk())
  {
    size_t n = stringCollection.Count();
    sqlite3_chararray* pCharArray = (sqlite3_chararray*) m_data;
    if (pCharArray->a != NULL && pCharArray->xFree)
    {
      pCharArray->xFree(pCharArray->a);
    }
    pCharArray->n = n;
    if (n > 0)
    {
      pCharArray->a = (char**) sqlite3_malloc(sizeof(char*)*n);
      pCharArray->xFree = sqlite3_free;
    }
    else
    {
      pCharArray->a = NULL;
      pCharArray->xFree = NULL;
    }

    size_t j;
    for (j = 0; j < n; ++j)
    {
      wxCharBuffer strValue = stringCollection[j].ToUTF8();
      const char* zValue = strValue;
      size_t k = strlen(zValue) + 1;
      pCharArray->a[j] = (char*) sqlite3_malloc(sizeof(char)*k);
      strcpy(pCharArray->a[j], zValue);
    }
  }
  else
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_INVALID_COLLECTION);
  }
}

wxSQLite3StringCollection
wxSQLite3Database::CreateStringCollection(const wxString& collectionName)
{
#if WXSQLITE3_USE_NAMED_COLLECTIONS
  CheckDatabase();
  int rc = SQLITE_OK;
  wxCharBuffer strCollectionName = collectionName.ToUTF8();
  const char* zName = strCollectionName;
  sqlite3_chararray* p = (sqlite3_chararray*) sqlite3_malloc( sizeof(*p) );
  if (p == 0)
  {
    throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOMEM);
  }
  p->n = 0;
  p->a= NULL;
  p->xFree = NULL;
  rc = sqlite3_create_module_v2(m_db->m_db, zName, &chararrayModule, p, (void(*)(void*))chararrayFree);
  if (rc == SQLITE_OK)
  {
    wxSQLite3StatementBuffer zBuffer;
    const char* zSql = zBuffer.Format("CREATE VIRTUAL TABLE temp.\"%w\" USING \"%w\"", zName, zName);
    rc = sqlite3_exec(m_db->m_db, zSql, 0, 0, 0);
  }
  if (rc != SQLITE_OK)
  {
    const char* localError = sqlite3_errmsg(m_db->m_db);
    throw wxSQLite3Exception(rc, wxString::FromUTF8(localError));
  }
  return wxSQLite3StringCollection(collectionName, p);
#else
  wxUnusedVar(collectionName);
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_NOCOLLECTIONS);
#endif // WXSQLITE3_USE_NAMED_COLLECTIONS
}

// --- Cipher support ---

wxSQLite3Cipher::wxSQLite3Cipher()
  : m_initialized(false), m_cipherType(WXSQLITE_CIPHER_UNKNOWN), m_legacyPageSize(0)
{
}

wxSQLite3Cipher::wxSQLite3Cipher(wxSQLite3CipherType cipherType)
  : m_initialized(false), m_cipherType(cipherType), m_legacyPageSize(0)
{
}

wxSQLite3Cipher::wxSQLite3Cipher(const wxSQLite3Cipher&  cipher)
  : m_initialized(cipher.m_initialized), m_cipherType(cipher.m_cipherType), m_legacyPageSize(cipher.m_legacyPageSize)
{
}

/// Destructor
wxSQLite3Cipher::~wxSQLite3Cipher()
{
}

bool
wxSQLite3Cipher::InitializeFromGlobalDefault()
{
  return false;
}

bool
wxSQLite3Cipher::InitializeFromCurrent(wxSQLite3Database& db)
{
  return false;
}

bool
wxSQLite3Cipher::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
  return false;
}

bool
wxSQLite3Cipher::Apply(wxSQLite3Database& db) const
{
  return false;
}

bool
wxSQLite3Cipher::Apply(void* dbHandle) const
{
  return false;
}

void
wxSQLite3Cipher::SetLegacyPageSize(int pageSize)
{
  if (pageSize >= 512 && pageSize <= 65536 && ((pageSize - 1) & pageSize) == 0)
  {
    m_legacyPageSize = pageSize;
  }
  else
  {
    m_legacyPageSize = 0;
  }
}

int
wxSQLite3Cipher::GetLegacyPageSize() const
{
  return m_legacyPageSize;
}

wxSQLite3CipherType
wxSQLite3Cipher::GetCipherType() const
{
  return m_cipherType;
}

bool
wxSQLite3Cipher::IsOk() const
{
  return (m_initialized && m_cipherType != WXSQLITE_CIPHER_UNKNOWN);
}

const wxString
wxSQLite3Cipher::GetCipherName(wxSQLite3CipherType cipherType)
{
  wxString cipherName;
  switch (cipherType)
  {
    case WXSQLITE_CIPHER_AES128:    cipherName = wxS("aes128cbc"); break;
    case WXSQLITE_CIPHER_AES256:    cipherName = wxS("aes256cbc"); break;
    case WXSQLITE_CIPHER_CHACHA20:  cipherName = wxS("chacha20");  break;
    case WXSQLITE_CIPHER_SQLCIPHER: cipherName = wxS("sqlcipher"); break;
    case WXSQLITE_CIPHER_RC4:       cipherName = wxS("rc4");       break;
    case WXSQLITE_CIPHER_ASCON128:  cipherName = wxS("ascon128");  break;
    case WXSQLITE_CIPHER_AEGIS:     cipherName = wxS("aegis");     break;
    default:                        cipherName = wxS("unknown");   break;
  }
  return cipherName;
}

wxSQLite3CipherType
wxSQLite3Cipher::GetCipherType(const wxString& cipherName)
{
  wxSQLite3CipherType cipherType;
  if (cipherName.IsSameAs(wxS("aes128cbc"), false))      cipherType = WXSQLITE_CIPHER_AES128;
  else if (cipherName.IsSameAs(wxS("aes256cbc"), false)) cipherType = WXSQLITE_CIPHER_AES256;
  else if (cipherName.IsSameAs(wxS("chacha20"), false))  cipherType = WXSQLITE_CIPHER_CHACHA20;
  else if (cipherName.IsSameAs(wxS("sqlcipher"), false)) cipherType = WXSQLITE_CIPHER_SQLCIPHER;
  else if (cipherName.IsSameAs(wxS("rc4"), false))       cipherType = WXSQLITE_CIPHER_RC4;
  else if (cipherName.IsSameAs(wxS("ascon128"), false))  cipherType = WXSQLITE_CIPHER_ASCON128;
  else if (cipherName.IsSameAs(wxS("aegis"), false))     cipherType = WXSQLITE_CIPHER_AEGIS;
  else                                                   cipherType = WXSQLITE_CIPHER_UNKNOWN;
  return cipherType;
}

bool
wxSQLite3Cipher::SetCipher(wxSQLite3Database& db, wxSQLite3CipherType cipherType)
{
  wxCharBuffer strCipherName = GetCipherName(cipherType).utf8_str();
  const char* cipherName = strCipherName;
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int newCipherType = (dbHandle != NULL) ? sqlite3mc_config(dbHandle, "cipher", sqlite3mc_cipher_index(cipherName)) : WXSQLITE_CIPHER_UNKNOWN;
  return (newCipherType > 0 && newCipherType == (int) cipherType && newCipherType != WXSQLITE_CIPHER_UNKNOWN);
}

bool
wxSQLite3Cipher::SetCipherDefault(wxSQLite3Database& db, wxSQLite3CipherType cipherType)
{
  wxCharBuffer strCipherName = GetCipherName(cipherType).utf8_str();
  const char* cipherName = strCipherName;
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int newCipherType = (dbHandle != NULL) ? sqlite3mc_config(dbHandle, "default:cipher", sqlite3mc_cipher_index(cipherName)) : WXSQLITE_CIPHER_UNKNOWN;
  return (newCipherType > 0 && newCipherType == (int) cipherType && newCipherType != WXSQLITE_CIPHER_UNKNOWN);
}

wxSQLite3CipherType
wxSQLite3Cipher::GetCipher(wxSQLite3Database& db)
{
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int cipherType = sqlite3mc_config(dbHandle, "cipher", -1);
  return GetCipherType(sqlite3mc_cipher_name(cipherType));
}

wxSQLite3CipherType
wxSQLite3Cipher::GetCipherDefault(wxSQLite3Database& db)
{
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int cipherType = sqlite3mc_config(dbHandle, "default:cipher", -1);
  return GetCipherType(sqlite3mc_cipher_name(cipherType));
}

wxSQLite3CipherType
wxSQLite3Cipher::GetGlobalCipherDefault()
{
  int cipherType = sqlite3mc_config(0, "default:cipher", -1);
  return GetCipherType(sqlite3mc_cipher_name(cipherType));
}

int
wxSQLite3Cipher::GetCipherParameterMin(const wxString& cipherName, const wxString& paramName)
{
  wxCharBuffer strCipherName = cipherName.ToUTF8();
  const char* zCipherName = strCipherName;
  wxString minParamName = wxString(wxS("min:")) + paramName;
  wxCharBuffer strParamName = minParamName.ToUTF8();
  const char* zParamName = strParamName;
  return sqlite3mc_config_cipher(0, zCipherName, zParamName, -1);
}

int
wxSQLite3Cipher::GetCipherParameterMax(const wxString& cipherName, const wxString& paramName)
{
  wxCharBuffer strCipherName = cipherName.ToUTF8();
  const char* zCipherName = strCipherName;
  wxString maxParamName = wxString(wxS("max:")) + paramName;
  wxCharBuffer strParamName = maxParamName.ToUTF8();
  const char* zParamName = strParamName;
  return sqlite3mc_config_cipher(0, zCipherName, zParamName, -1);
}

void
wxSQLite3Cipher::SetInitialized(bool initialized)
{
  m_initialized = initialized;
}

void
wxSQLite3Cipher::SetCipherType(wxSQLite3CipherType cipherType)
{
  m_cipherType = cipherType;
}

void*
wxSQLite3Cipher::GetDatabaseHandle(wxSQLite3Database& db)
{
  return db.GetDatabaseHandle();
}


wxSQLite3CipherAes128::wxSQLite3CipherAes128()
  : wxSQLite3Cipher(WXSQLITE_CIPHER_AES128), m_legacy(false)
{
  SetInitialized(true);
}

wxSQLite3CipherAes128::wxSQLite3CipherAes128(const wxSQLite3CipherAes128&  cipher)
  : wxSQLite3Cipher(cipher), m_legacy(cipher.m_legacy)
{
}

wxSQLite3CipherAes128::~wxSQLite3CipherAes128()
{
}

bool
wxSQLite3CipherAes128::InitializeFromGlobalDefault()
{
#if HAVE_CIPHER_AES_128_CBC
  int legacy = sqlite3mc_config_cipher(0, "aes128cbc", "legacy", -1);
  m_legacy = legacy != 0;
  bool initialized = legacy >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAes128::InitializeFromCurrent(wxSQLite3Database& db)
{
#if HAVE_CIPHER_AES_128_CBC
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "aes128cbc", "legacy", -1);
  m_legacy = legacy != 0;
  bool initialized = legacy >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAes128::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
#if HAVE_CIPHER_AES_128_CBC
  sqlite3* dbHandle = (sqlite3*)GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "aes128cbc", "default:legacy", -1);
  m_legacy = legacy != 0;
  bool initialized = legacy >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAes128::Apply(wxSQLite3Database& db) const
{
  return Apply(GetDatabaseHandle(db));
}

bool
wxSQLite3CipherAes128::Apply(void* dbHandle) const
{
#if HAVE_CIPHER_AES_128_CBC
  bool applied = false;
  if (IsOk())
  {
    if (dbHandle != NULL)
    {
      int newCipherType = sqlite3mc_config((sqlite3*) dbHandle, "cipher", sqlite3mc_cipher_index("aes128cbc"));
      int legacy = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aes128cbc", "legacy", (m_legacy) ? 1 : 0);
      int legacyPageSize = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aes128cbc", "legacy_page_size", GetLegacyPageSize());
      applied = (newCipherType > 0) && (legacy >= 0) && (legacyPageSize >= 0);
    }
  }
  return applied;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}


wxSQLite3CipherAes256::wxSQLite3CipherAes256()
  : wxSQLite3Cipher(WXSQLITE_CIPHER_AES256), m_legacy(false), m_kdfIter(4001)
{
  SetInitialized(true);
}

wxSQLite3CipherAes256::wxSQLite3CipherAes256(const wxSQLite3CipherAes256& cipher)
  : wxSQLite3Cipher(cipher), m_legacy(cipher.m_legacy), m_kdfIter(cipher.m_kdfIter)
{
}

wxSQLite3CipherAes256::~wxSQLite3CipherAes256()
{
}

bool
wxSQLite3CipherAes256::InitializeFromGlobalDefault()
{
#if HAVE_CIPHER_AES_256_CBC
  int legacy = sqlite3mc_config_cipher(0, "aes256cbc", "legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(0, "aes256cbc", "kdf_iter", -1);
  bool initialized = legacy >= 0 && m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAes256::InitializeFromCurrent(wxSQLite3Database& db)
{
#if HAVE_CIPHER_AES_256_CBC
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "aes256cbc", "legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "aes256cbc", "kdf_iter", -1);
  bool initialized = legacy >= 0 && m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAes256::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
#if HAVE_CIPHER_AES_256_CBC
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "aes256cbc", "default:legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "aes256cbc", "default:kdf_iter", -1);
  bool initialized = legacy >= 0 && m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAes256::Apply(wxSQLite3Database& db) const
{
  return Apply(GetDatabaseHandle(db));
}

bool
wxSQLite3CipherAes256::Apply(void* dbHandle) const
{
#if HAVE_CIPHER_AES_256_CBC
  bool applied = false;
  if (IsOk())
  {
    if (dbHandle != NULL)
    {
      int newCipherType = sqlite3mc_config((sqlite3*) dbHandle, "cipher", sqlite3mc_cipher_index("aes256cbc"));
      int legacy = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aes256cbc", "legacy", (m_legacy) ? 1 : 0);
      int legacyPageSize = sqlite3mc_config_cipher((sqlite3*)dbHandle, "aes256cbc", "legacy_page_size", GetLegacyPageSize());
      int kdfIter = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aes256cbc", "kdf_iter", m_kdfIter);
      applied = (newCipherType > 0) && (legacy >= 0) && (legacyPageSize >= 0) && (kdfIter > 0);
    }
  }
  return applied;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

wxSQLite3CipherChaCha20::wxSQLite3CipherChaCha20()
  : wxSQLite3Cipher(WXSQLITE_CIPHER_CHACHA20), m_legacy(false), m_kdfIter(64007)
{
  SetInitialized(true);
}

wxSQLite3CipherChaCha20::wxSQLite3CipherChaCha20(const wxSQLite3CipherChaCha20&  cipher)
  : wxSQLite3Cipher(cipher), m_legacy(cipher.m_legacy), m_kdfIter(cipher.m_kdfIter)
{
}

wxSQLite3CipherChaCha20::~wxSQLite3CipherChaCha20()
{
}

bool
wxSQLite3CipherChaCha20::InitializeFromGlobalDefault()
{
#if HAVE_CIPHER_CHACHA20
  int legacy = sqlite3mc_config_cipher(0, "chacha20", "legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(0, "chacha20", "kdf_iter", -1);
  bool initialized = legacy >= 0 && m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherChaCha20::InitializeFromCurrent(wxSQLite3Database& db)
{
#if HAVE_CIPHER_CHACHA20
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "chacha20", "legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "chacha20", "kdf_iter", -1);
  bool initialized = legacy >= 0 && m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherChaCha20::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
#if HAVE_CIPHER_CHACHA20
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "chacha20", "default:legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "chacha20", "default:kdf_iter", -1);
  bool initialized = legacy >= 0 && m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherChaCha20::Apply(wxSQLite3Database& db) const
{
  return Apply(GetDatabaseHandle(db));
}

bool
wxSQLite3CipherChaCha20::Apply(void* dbHandle) const
{
#if HAVE_CIPHER_CHACHA20
  bool applied = false;
  if (IsOk())
  {
    if (dbHandle != NULL)
    {
      int newCipherType = sqlite3mc_config((sqlite3*) dbHandle, "cipher", sqlite3mc_cipher_index("chacha20"));
      int legacy = sqlite3mc_config_cipher((sqlite3*) dbHandle, "chacha20", "legacy", (m_legacy) ? 1 : 0);
      int legacyPageSize = sqlite3mc_config_cipher((sqlite3*) dbHandle, "chacha20", "legacy_page_size", GetLegacyPageSize());
      int kdfIter = sqlite3mc_config_cipher((sqlite3*) dbHandle, "chacha20", "kdf_iter", m_kdfIter);
      applied = (newCipherType > 0) && (legacy >= 0) && (legacyPageSize >= 0) && (kdfIter > 0);
    }
  }
  return applied;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}


wxSQLite3CipherSQLCipher::wxSQLite3CipherSQLCipher()
  : wxSQLite3Cipher(WXSQLITE_CIPHER_SQLCIPHER), m_legacy(false), m_legacyVersion(0), m_kdfIter(256000),
                    m_fastKdfIter(2), m_hmacUse(true), m_hmacPgNo(1), m_hmacSaltMask(0x3a),
                    m_kdfAlgorithm(ALGORITHM_SHA512), m_hmacAlgorithm(ALGORITHM_SHA512)
{
  SetInitialized(true);
}

wxSQLite3CipherSQLCipher::wxSQLite3CipherSQLCipher(const wxSQLite3CipherSQLCipher&  cipher)
  : wxSQLite3Cipher(cipher), m_legacy(cipher.m_legacy), m_legacyVersion(cipher.m_legacyVersion), m_kdfIter(cipher.m_kdfIter),
    m_fastKdfIter(cipher.m_fastKdfIter), m_hmacUse(cipher.m_hmacUse), 
    m_hmacPgNo(cipher.m_hmacPgNo), m_hmacSaltMask(cipher.m_hmacSaltMask),
    m_kdfAlgorithm(cipher.m_kdfAlgorithm), m_hmacAlgorithm(cipher.m_hmacAlgorithm)
{
}

wxSQLite3CipherSQLCipher::~wxSQLite3CipherSQLCipher()
{
}

bool
wxSQLite3CipherSQLCipher::InitializeFromGlobalDefault()
{
#if HAVE_CIPHER_SQLCIPHER
  int legacy = sqlite3mc_config_cipher(0, "sqlcipher", "legacy", -1);
  m_legacy = legacy != 0;
  m_legacyVersion = legacy;
  m_kdfIter = sqlite3mc_config_cipher(0, "sqlcipher", "kdf_iter", -1);
  m_fastKdfIter = sqlite3mc_config_cipher(0, "sqlcipher", "fast_kdf_iter", -1);
  int hmacUse = sqlite3mc_config_cipher(0, "sqlcipher", "hmac_use", -1);
  m_hmacUse = hmacUse != 0;
  m_hmacPgNo = sqlite3mc_config_cipher(0, "sqlcipher", "hmac_pgno", -1);
  m_hmacSaltMask = sqlite3mc_config_cipher(0, "sqlcipher", "hmac_salt_mask", -1);
  int kdfAlgorithm = sqlite3mc_config_cipher(0, "sqlcipher", "kdf_algorithm", -1);
  if (kdfAlgorithm >= 0) m_kdfAlgorithm = (Algorithm) kdfAlgorithm;
  int hmacAlgorithm = sqlite3mc_config_cipher(0, "sqlcipher", "hmac_algorithm", -1);
  if (hmacAlgorithm >= 0) m_hmacAlgorithm = (Algorithm) hmacAlgorithm;
  bool initialized = legacy >= 0 && m_kdfIter > 0 && m_fastKdfIter > 0 &&
                     hmacUse >= 0 && m_hmacPgNo >= 0 && m_hmacSaltMask >= 0 &&
                     kdfAlgorithm >= 0 && hmacAlgorithm >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherSQLCipher::InitializeFromCurrent(wxSQLite3Database& db)
{
#if HAVE_CIPHER_SQLCIPHER
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "kdf_iter", -1);
  m_fastKdfIter = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "fast_kdf_iter", -1);
  int hmacUse = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "hmac_use", -1);
  m_hmacUse = hmacUse != 0;
  m_hmacPgNo = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "hmac_pgno", -1);
  m_hmacSaltMask = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "hmac_salt_mask", -1);
  int kdfAlgorithm = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "kdf_algorithm", -1);
  if (kdfAlgorithm >= 0) m_kdfAlgorithm = (Algorithm)kdfAlgorithm;
  int hmacAlgorithm = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "hmac_algorithm", -1);
  if (hmacAlgorithm >= 0) m_hmacAlgorithm = (Algorithm)hmacAlgorithm;
  bool initialized = legacy >= 0 && m_kdfIter > 0 && m_fastKdfIter > 0 &&
                     hmacUse >= 0 && m_hmacPgNo >= 0 && m_hmacSaltMask >= 0 &&
                     kdfAlgorithm >= 0 && hmacAlgorithm >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherSQLCipher::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
#if HAVE_CIPHER_SQLCIPHER
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:legacy", -1);
  m_legacy = legacy != 0;
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:kdf_iter", -1);
  m_fastKdfIter = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:fast_kdf_iter", -1);
  int hmacUse = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:hmac_use", -1);
  m_hmacUse = hmacUse != 0;
  m_hmacPgNo = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:hmac_pgno", -1);
  m_hmacSaltMask = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:hmac_salt_mask", -1);
  int kdfAlgorithm = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:kdf_algorithm", -1);
  if (kdfAlgorithm >= 0) m_kdfAlgorithm = (Algorithm)kdfAlgorithm;
  int hmacAlgorithm = sqlite3mc_config_cipher(dbHandle, "sqlcipher", "default:hmac_algorithm", -1);
  if (hmacAlgorithm >= 0) m_hmacAlgorithm = (Algorithm)hmacAlgorithm;
  bool initialized = legacy >= 0 && m_kdfIter > 0 && m_fastKdfIter > 0 &&
                     hmacUse >= 0 && m_hmacPgNo >= 0 && m_hmacSaltMask >= 0 &&
                     kdfAlgorithm >= 0 && hmacAlgorithm >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherSQLCipher::Apply(wxSQLite3Database& db) const
{
  return Apply(GetDatabaseHandle(db));
}

bool
wxSQLite3CipherSQLCipher::Apply(void* dbHandle) const
{
#if HAVE_CIPHER_SQLCIPHER
  bool applied = false;
  if (IsOk())
  {
    if (dbHandle != NULL)
    {
      int newCipherType = sqlite3mc_config((sqlite3*) dbHandle, "cipher", sqlite3mc_cipher_index("sqlcipher"));
      int legacy = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "legacy", (m_legacy) ? 1 : 0);
      int legacyPageSize = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "legacy_page_size", GetLegacyPageSize());
      int kdfIter = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "kdf_iter", m_kdfIter);
      int fastKdfIter = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "fast_kdf_iter", m_fastKdfIter);
      int hmacUse = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "hmac_use", (m_hmacUse) ? 1 : 0);
      int hmacPgNo = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "hmac_pgno", m_hmacPgNo);
      int hmacSaltMask = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "hmac_salt_mask", m_hmacSaltMask);
      int kdfAlgorithm = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "kdf_algorithm", (int) m_kdfAlgorithm);
      int hmacAlgorithm = sqlite3mc_config_cipher((sqlite3*) dbHandle, "sqlcipher", "hmac_algorithm", (int) m_hmacAlgorithm);
      applied = (newCipherType > 0) && (legacy >= 0) && (legacyPageSize >= 0) &&
                (kdfIter > 0) && (fastKdfIter > 0) &&
                (hmacUse >= 0) && (hmacPgNo >= 0) && (hmacSaltMask >= 0) &&
                (kdfAlgorithm >= 0) && (hmacAlgorithm >= 0);
    }
  }
  return applied;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

void
wxSQLite3CipherSQLCipher::InitializeVersionDefault(int version)
{
#if HAVE_CIPHER_SQLCIPHER
  switch (version)
  {
    case 1:
      m_legacy = true;
      m_legacyVersion = 1;
      m_kdfIter = 4000;
      m_fastKdfIter = 2;
      m_hmacUse = false;
      m_hmacPgNo = 1;
      m_hmacSaltMask = 0x3a;
      m_kdfAlgorithm = ALGORITHM_SHA1;
      m_hmacAlgorithm = ALGORITHM_SHA1;
      SetLegacyPageSize(1024);
      break;
    case 2:
      m_legacy = true;
      m_legacyVersion = 2;
      m_kdfIter = 4000;
      m_fastKdfIter = 2;
      m_hmacUse = true;
      m_hmacPgNo = 1;
      m_hmacSaltMask = 0x3a;
      m_kdfAlgorithm = ALGORITHM_SHA1;
      m_hmacAlgorithm = ALGORITHM_SHA1;
      SetLegacyPageSize(1024);
      break;
    case 3:
      m_legacy = true;
      m_legacyVersion = 3;
      m_kdfIter = 64000;
      m_fastKdfIter = 2;
      m_hmacUse = true;
      m_hmacPgNo = 1;
      m_hmacSaltMask = 0x3a;
      m_kdfAlgorithm = ALGORITHM_SHA1;
      m_hmacAlgorithm = ALGORITHM_SHA1;
      SetLegacyPageSize(1024);
      break;
    case 4:
    default:
      m_legacy = true;
      m_legacyVersion = 4;
      m_kdfIter = 256000;
      m_fastKdfIter = 2;
      m_hmacUse = true;
      m_hmacPgNo = 1;
      m_hmacSaltMask = 0x3a;
      m_kdfAlgorithm = ALGORITHM_SHA512;
      m_hmacAlgorithm = ALGORITHM_SHA512;
      SetLegacyPageSize(4096);
      break;
  }
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

wxSQLite3CipherRC4::wxSQLite3CipherRC4()
  : wxSQLite3Cipher(WXSQLITE_CIPHER_RC4), m_legacy(true)
{
  SetInitialized(true);
}

wxSQLite3CipherRC4::wxSQLite3CipherRC4(const wxSQLite3CipherRC4&  cipher)
  : wxSQLite3Cipher(cipher), m_legacy(cipher.m_legacy)
{
}

wxSQLite3CipherRC4::~wxSQLite3CipherRC4()
{
}

bool
wxSQLite3CipherRC4::InitializeFromGlobalDefault()
{
#if HAVE_CIPHER_RC4
  int legacy = sqlite3mc_config_cipher(0, "rc4", "legacy", -1);
  m_legacy = legacy != 0;
  bool initialized = legacy >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherRC4::InitializeFromCurrent(wxSQLite3Database& db)
{
#if HAVE_CIPHER_RC4
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "rc4", "legacy", -1);
  m_legacy = legacy != 0;
  bool initialized = legacy >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherRC4::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
#if HAVE_CIPHER_RC4
  sqlite3* dbHandle = (sqlite3*)GetDatabaseHandle(db);
  int legacy = sqlite3mc_config_cipher(dbHandle, "rc4", "default:legacy", -1);
  m_legacy = legacy != 0;
  bool initialized = legacy >= 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherRC4::Apply(wxSQLite3Database& db) const
{
  return Apply(GetDatabaseHandle(db));
}

bool
wxSQLite3CipherRC4::Apply(void* dbHandle) const
{
#if HAVE_CIPHER_RC4
  bool applied = false;
  if (IsOk())
  {
    if (dbHandle != NULL)
    {
      int newCipherType = sqlite3mc_config((sqlite3*) dbHandle, "cipher", sqlite3mc_cipher_index("rc4"));
      int legacy = sqlite3mc_config_cipher((sqlite3*) dbHandle, "rc4", "legacy", (m_legacy) ? 1 : 0);
      int legacyPageSize = sqlite3mc_config_cipher((sqlite3*) dbHandle, "rc4", "legacy_page_size", GetLegacyPageSize());
      applied = (newCipherType > 0) && (legacy >= 0) && (legacyPageSize >= 0);
    }
  }
  return applied;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

wxSQLite3CipherAscon128::wxSQLite3CipherAscon128()
  : wxSQLite3Cipher(WXSQLITE_CIPHER_ASCON128), m_legacy(false), m_kdfIter(64007)
{
  SetInitialized(true);
}

wxSQLite3CipherAscon128::wxSQLite3CipherAscon128(const wxSQLite3CipherAscon128&  cipher)
  : wxSQLite3Cipher(cipher), m_legacy(cipher.m_legacy), m_kdfIter(cipher.m_kdfIter)
{
}

wxSQLite3CipherAscon128::~wxSQLite3CipherAscon128()
{
}

bool
wxSQLite3CipherAscon128::InitializeFromGlobalDefault()
{
#if HAVE_CIPHER_ASCON128
#if 0
  int legacy = sqlite3mc_config_cipher(0, "ascon128", "legacy", -1);
  m_legacy = legacy != 0;
#endif
  m_kdfIter = sqlite3mc_config_cipher(0, "ascon128", "kdf_iter", -1);
  bool initialized = m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAscon128::InitializeFromCurrent(wxSQLite3Database& db)
{
#if HAVE_CIPHER_ASCON128
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
#if 0
  int legacy = sqlite3mc_config_cipher(dbHandle, "ascon128", "legacy", -1);
  m_legacy = legacy != 0;
#endif
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "ascon128", "kdf_iter", -1);
  bool initialized = m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAscon128::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
#if HAVE_CIPHER_ASCON128
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
#if 0
  int legacy = sqlite3mc_config_cipher(dbHandle, "ascon128", "default:legacy", -1);
  m_legacy = legacy != 0;
#endif
  m_kdfIter = sqlite3mc_config_cipher(dbHandle, "ascon128", "default:kdf_iter", -1);
  bool initialized = m_kdfIter > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAscon128::Apply(wxSQLite3Database& db) const
{
  return Apply(GetDatabaseHandle(db));
}

bool
wxSQLite3CipherAscon128::Apply(void* dbHandle) const
{
#if HAVE_CIPHER_ASCON128
  bool applied = false;
  if (IsOk())
  {
    if (dbHandle != NULL)
    {
      int newCipherType = sqlite3mc_config((sqlite3*) dbHandle, "cipher", sqlite3mc_cipher_index("ascon128"));
#if 0
      int legacy = sqlite3mc_config_cipher((sqlite3*) dbHandle, "ascon128", "legacy", (m_legacy) ? 1 : 0);
      int legacyPageSize = sqlite3mc_config_cipher((sqlite3*) dbHandle, "ascon128", "legacy_page_size", GetLegacyPageSize());
#endif
      int kdfIter = sqlite3mc_config_cipher((sqlite3*) dbHandle, "ascon128", "kdf_iter", m_kdfIter);
      applied = (newCipherType > 0) /* && (legacy >= 0) && (legacyPageSize >= 0) */ && (kdfIter > 0);
    }
  }
  return applied;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

wxSQLite3CipherAegis::wxSQLite3CipherAegis()
  : wxSQLite3Cipher(WXSQLITE_CIPHER_AEGIS), m_legacy(false),
    m_tcost(2), m_mcost(19*1024), m_pcost(1), m_algorithm(ALGORITHM_AEGIS_256)
{
  SetInitialized(true);
}

wxSQLite3CipherAegis::wxSQLite3CipherAegis(const wxSQLite3CipherAegis&  cipher)
  : wxSQLite3Cipher(cipher), m_legacy(cipher.m_legacy),
    m_tcost(cipher.m_tcost), m_mcost(cipher.m_mcost), m_pcost(cipher.m_pcost),
    m_algorithm(cipher.m_algorithm)
{
}

wxSQLite3CipherAegis::~wxSQLite3CipherAegis()
{
}

bool
wxSQLite3CipherAegis::InitializeFromGlobalDefault()
{
#if HAVE_CIPHER_AEGIS
#if 0
  int legacy = sqlite3mc_config_cipher(0, "aegis", "legacy", -1);
  m_legacy = legacy != 0;
#endif
  m_tcost = sqlite3mc_config_cipher(0, "aegis", "tcost", -1);
  m_mcost = sqlite3mc_config_cipher(0, "aegis", "mcost", -1);
  m_pcost = sqlite3mc_config_cipher(0, "aegis", "pcost", -1);
  int algorithm = (Algorithm)sqlite3mc_config_cipher(0, "aegis", "algorithm", -1);
  if (algorithm > 0) m_algorithm = (Algorithm)algorithm;
  bool initialized = m_tcost > 0 && m_mcost > 0 && m_pcost > 0 && m_algorithm > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAegis::InitializeFromCurrent(wxSQLite3Database& db)
{
#if HAVE_CIPHER_AEGIS
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
#if 0
  int legacy = sqlite3mc_config_cipher(dbHandle, "aegis", "legacy", -1);
  m_legacy = legacy != 0;
#endif
  m_tcost = sqlite3mc_config_cipher(dbHandle, "aegis", "tcost", -1);
  m_mcost = sqlite3mc_config_cipher(dbHandle, "aegis", "mcost", -1);
  m_pcost = sqlite3mc_config_cipher(dbHandle, "aegis", "pcost", -1);
  int algorithm = (Algorithm) sqlite3mc_config_cipher(dbHandle, "aegis", "algorithm", -1);
  if (algorithm > 0) m_algorithm = (Algorithm) algorithm;
  bool initialized = m_tcost > 0 && m_mcost > 0 && m_pcost > 0 && m_algorithm > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAegis::InitializeFromCurrentDefault(wxSQLite3Database& db)
{
#if HAVE_CIPHER_AEGIS
  sqlite3* dbHandle = (sqlite3*) GetDatabaseHandle(db);
#if 0
  int legacy = sqlite3mc_config_cipher(dbHandle, "aegis", "default:legacy", -1);
  m_legacy = legacy != 0;
#endif
  m_tcost = sqlite3mc_config_cipher(dbHandle, "aegis", "default:tcost", -1);
  m_mcost = sqlite3mc_config_cipher(dbHandle, "aegis", "default:mcost", -1);
  m_pcost = sqlite3mc_config_cipher(dbHandle, "aegis", "default:pcost", -1);
  int algorithm = sqlite3mc_config_cipher(dbHandle, "aegis", "default:algorithm", -1);
  if (algorithm > 0) m_algorithm = (Algorithm) algorithm;
  bool initialized = m_tcost > 0 && m_mcost > 0 && m_pcost > 0 && m_algorithm > 0;
  SetInitialized(initialized);
  return initialized;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}

bool
wxSQLite3CipherAegis::Apply(wxSQLite3Database& db) const
{
  return Apply(GetDatabaseHandle(db));
}

bool
wxSQLite3CipherAegis::Apply(void* dbHandle) const
{
#if HAVE_CIPHER_AEGIS
  bool applied = false;
  if (IsOk())
  {
    if (dbHandle != NULL)
    {
      int newCipherType = sqlite3mc_config((sqlite3*) dbHandle, "cipher", sqlite3mc_cipher_index("aegis"));
#if 0
      int legacy = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aegis", "legacy", (m_legacy) ? 1 : 0);
      int legacyPageSize = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aegis", "legacy_page_size", GetLegacyPageSize());
#endif
      int tcost = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aegis", "tcost", m_tcost);
      int mcost = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aegis", "mcost", m_mcost);
      int pcost = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aegis", "pcost", m_pcost);
      int algorithm = sqlite3mc_config_cipher((sqlite3*) dbHandle, "aegis", "algorithm", (int) m_algorithm);
      applied = (newCipherType > 0) && (tcost > 0) && (mcost > 0) && (pcost > 0) && (algorithm > 0);
    }
  }
  return applied;
#else
  throw wxSQLite3Exception(WXSQLITE_ERROR, wxERRMSG_CIPHER_NOT_SUPPORTED);
#endif
}
