// TortoiseSVN - a Windows shell extension for easy version control

// Copyright (C) 2003-2016 - TortoiseSVN

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#pragma warning(push)
#include "svn_wc.h"
#pragma warning(pop)
#include "SVNBase.h"
#include "SVNPrompt.h"
#include "SVNRev.h"
#include "SVNGlobal.h"
#include "SVNExternals.h"
#include "ProjectProperties.h"

#include "ILogReceiver.h"
#include "LogCachePool.h"
#include "CacheLogQuery.h"

class CProgressDlg;
class CTSVNPath;
class CTSVNPathList;

namespace async
{
    class CCriticalSection;
}

svn_error_t * svn_error_handle_malfunction(svn_boolean_t can_return,
                                           const char *file, int line,
                                           const char *expr);

svn_error_t * svn_cl__get_log_message (const char **log_msg,
                                    const char **tmp_file,
                                    const apr_array_header_t * commit_items,
                                    void *baton, apr_pool_t * pool);
#define SVN_PROGRESS_QUEUE_SIZE 10
#define SVN_DATE_BUFFER 260

typedef std::map<CString, CString> RevPropHash;

/**
 * \ingroup SVN
 * This class provides all Subversion commands as methods and adds some helper
 * methods to the Subversion commands. Also provides virtual methods for the
 * callbacks Subversion uses. This class can't be used directly but must be
 * inherited if the callbacks are used.
 *
 * \remark Please note that all URLs passed to any of the methods of this class
 * must not be escaped! Escaping and unescaping of URLs is done automatically.
 * The drawback of this is that valid pathnames with sequences looking like escaped
 * chars may not work correctly under certain circumstances.
 */
class SVN : public SVNBase, private ILogReceiver
{
public:
    SVN(const SVN&) = delete;
    SVN& operator=(SVN&) = delete;
    SVN(bool suppressUI = false);
    virtual ~SVN(void);

    virtual BOOL Cancel();
    virtual BOOL Notify(const CTSVNPath& path, const CTSVNPath& url, svn_wc_notify_action_t action,
                            svn_node_kind_t kind, const CString& mime_type,
                            svn_wc_notify_state_t content_state,
                            svn_wc_notify_state_t prop_state, svn_revnum_t rev,
                            const svn_lock_t * lock, svn_wc_notify_lock_state_t lock_state,
                            const CString& changelistname,
                            const CString& propertyName,
                            svn_merge_range_t * range,
                            svn_error_t * err, apr_pool_t * pool);
    virtual BOOL Log(svn_revnum_t rev, const std::string& author, const std::string& message, apr_time_t time, const MergeInfo* mergeInfo);
    virtual BOOL BlameCallback(LONG linenumber, bool localchange, svn_revnum_t revision, const CString& author, const CString& date,
                            svn_revnum_t merged_revision, const CString& merged_author, const CString& merged_date, const CString& merged_path,
                            const CStringA& line, const CStringA& log_msg, const CStringA& merged_log_msg);
    virtual svn_error_t* DiffSummarizeCallback(const CTSVNPath& path, svn_client_diff_summarize_kind_t kind, bool propchanged, svn_node_kind_t node);
    virtual BOOL ReportList(const CString& path, svn_node_kind_t kind,
                            svn_filesize_t size, bool has_props,
                            svn_revnum_t created_rev, apr_time_t time,
                            const CString& author,
                            const CString& locktoken, const CString& lockowner,
                            const CString& lockcomment, bool is_dav_comment,
                            apr_time_t lock_creationdate, apr_time_t lock_expirationdate,
                            const CString& absolutepath, const CString& externalParentUrl, const CString& externalTarget);
    virtual svn_wc_conflict_choice_t ConflictResolveCallback(const svn_wc_conflict_description2_t *description, CString& mergedfile);

    struct SVNLock
    {
        CString path;
        CString token;
        CString owner;
        CString comment;
        __time64_t creation_date;
        __time64_t expiration_date;

        /// Ensure a defined initial state
        SVNLock();
    };

    struct SVNProgress
    {
        apr_off_t progress;         ///< operation progress
        apr_off_t total;            ///< operation progress
        apr_off_t overall_total;    ///< total bytes transferred, use SetAndClearProgressInfo() to reset this
        apr_off_t BytesPerSecond;   ///< Speed in bytes per second
        CString   SpeedString;      ///< String for speed. Either "xxx Bytes/s" or "xxx kBytes/s"

        /// Ensure a defined initial state
        SVNProgress();
    };

    enum SVNExportType
    {
        SVNExportNormal,
        SVNExportIncludeUnversioned,
        SVNExportOnlyLocalChanges
    };

    /**
     * Checkout a working copy of moduleName at revision, using destPath as the root
     * directory of the newly checked out working copy
     * \param moduleName the path/url of the repository
     * \param destPath the path to the local working copy
     * \param revision the revision number to check out
     * \param pegrev the peg revision to use
     * \param depth the Subversion depth enum
     * \param bIgnoreExternals if TRUE, do not check out externals
     * \param bAllow_unver_obstructions if true then the checkout tolerates
     * existing unversioned items that obstruct added paths from @a moduleName.  Only
     * obstructions of the same type (file or dir) as the added item are
     * tolerated.  The text of obstructing files is left as-is, effectively
     * treating it as a user modification after the checkout.  Working
     * properties of obstructing items are set equal to the base properties.
     * If @a bAllow_unver_obstructions is false then the checkout will abort
     * if there are any unversioned obstructing items.
     * \return TRUE if successful
     */
    bool Checkout(const CTSVNPath& moduleName, const CTSVNPath& destPath, const SVNRev& pegrev,
        const SVNRev& revision, svn_depth_t depth, bool bIgnoreExternals,
        bool bAllow_unver_obstructions);
    /**
     * If path list contains an URL, use the MESSAGE to immediately attempt
     * to commit a deletion of the URL from the repository.
     * Else, schedule a working copy path for removal from the repository.
     * path's parent must be under revision control. This is just a
     * \b scheduling operation.  No changes will happen to the repository until
     * a commit occurs.  This scheduling can be removed with
     * Revert(). If path is a file it is immediately removed from the
     * working copy. If path is a directory it will remain in the working copy
     * but all the files, and all unversioned items, it contains will be
     * removed. If force is not set then this operation will fail if path
     * contains locally modified and/or unversioned items. If force is set such
     * items will be deleted.
     *
     * \param pathlist a list of files/directories to delete
     * \param force if TRUE, all files including those not versioned are deleted. If FALSE the operation
     * will fail if a directory contains unversioned files or if the file itself is not versioned.
     * \param keeplocal true means the file/dir is not removed from the working copy but only scheduled
     * for deletion in the repository. After the next commit, the file/dir will be unversioned.
     * \return TRUE if successful
     */
    bool Remove(const CTSVNPathList& pathlist, bool force, bool keeplocal = true, const CString& message = L"", const RevPropHash& revProps = RevPropHash());
    /**
     * Reverts a list of files/directories to its pristine state. I.e. its reverted to the state where it
     * was last updated with the repository.
     * \param pathlist the files/directories to revert
     * \param changelists
     * \param recurse
     * \return TRUE if successful
     */
    bool Revert(const CTSVNPathList& pathlist, const CStringArray& changelists, bool recurse, bool clearchangelists, bool metadataonly);
    /**
     * Schedule a working copy path for addition to the repository.
     * path's parent must be under revision control already, but path is
     * not. If recursive is set, then assuming path is a directory, all
     * of its contents will be scheduled for addition as well.
     * Important:  this is a \b scheduling operation.  No changes will
     * happen to the repository until a commit occurs.  This scheduling
     * can be removed with Revert().
     *
     * \param pathList the files/directories to add
     * \param props
     * \param depth
     * \param force
     * \param bUseAutoprops
     * \param no_ignore if FALSE, then don't add ignored files.
     * \param addparents if true, recurse up path's directory and look for a versioned directory.
     *                   If found, add all intermediate paths between it and path.
     * \return TRUE if successful
     */
    bool Add(const CTSVNPathList& pathList, ProjectProperties * props, svn_depth_t depth, bool force, bool bUseAutoprops, bool no_ignore, bool addparents);
    /**
     * Assigns the files/folders in \c pathList to a \c changelist.
     * \return TRUE if successful
     */
    bool AddToChangeList(const CTSVNPathList& pathList, const CString& changelist, svn_depth_t depth, const CStringArray& changelists = CStringArray());
    /**
     * Removes the files/folders in \c pathList from the \c changelist.
     * \return TRUE if successful
     */
    bool RemoveFromChangeList(const CTSVNPathList& pathList, const CStringArray& changelists, svn_depth_t depth);
    /**
     * Update working tree path to revision.
     * \param pathList the files/directories to update
     * \param revision the revision the local copy should be updated to or -1 for HEAD
     * \param depth the Subversion depth enum
     * \param depthIsSticky
     * \param ignoreexternals if TRUE, don't update externals
     * \param bAllow_unver_obstructions if true then the update tolerates
     * existing unversioned items that obstruct added paths from @a pathList.
     * Only obstructions of the same type (file or dir) as the added item are tolerated.
     * The text of obstructing files is left as-is, effectively
     * treating it as a user modification after the update.  Working
     * properties of obstructing items are set equal to the base properties.
     * \param makeParents
     * If @a bAllow_unver_obstructions is false then the update will abort
     * if there are any unversioned obstructing items.
     */
    bool Update(const CTSVNPathList& pathList, const SVNRev& revision, svn_depth_t depth,
        bool depthIsSticky, bool ignoreexternals, bool bAllow_unver_obstructions, bool makeParents);
    /**
     * Commit file or directory path into repository, using message as
     * the log message.
     *
     * If no error is returned and revision is set to -1, then the
     * commit was a no-op; nothing needed to be committed.
     *
     * If 0 is returned then the commit failed. Use GetLastErrorMessage()
     * to get detailed error information.
     *
     * \param pathlist the files/directories to commit
     * \param message a log message describing the changes you made
     * \param changelists If \c changelists is non-empty, then use it as a restrictive
     *                   filter on items that are committed; that is, don't commit
     *                   anything unless it's a member of changelist \c changelists.
     * \param keepchangelist After the commit completes successfully, remove \c changelist
     *                       associations from the targets, unless \c keepchangelist is set.
     * \param depth how deep to commit
     * \param keep_locks if TRUE, the locks are not removed on commit
     * \param revProps a hash of revision properties to set with the commit
     * \return the resulting revision number.
     */
    svn_revnum_t Commit(const CTSVNPathList& pathlist, const CString& message,
        const CStringArray& changelists, bool keepchangelist, svn_depth_t depth, bool keep_locks, const RevPropHash& revProps);
    /**
     * Copy srcPath to destPath.
     *
     * srcPath must be a file or directory under version control, or the
     * URL of a versioned item in the repository.  If srcPath is a URL,
     * revision is used to choose the revision from which to copy the
     * srcPath. destPath must be a file or directory under version
     * control, or a repository URL, existent or not.
     *
     * If either srcPath or destPath are URLs, immediately attempt
     * to commit the copy action in the repository.
     *
     * If neither srcPath nor destPath is a URL, then this is just a
     * variant of Add(), where the destPath items are scheduled
     * for addition as copies.  No changes will happen to the repository
     * until a commit occurs.  This scheduling can be removed with
     * Revert().
     *
     * \param srcPathList list of source paths to copy
     * \param destPath destination path
     * \param revision the revision of the source items
     * \param pegrev the peg revision
     * \param logmsg the log message to use if the copy is URL->URL
     * \param copy_as_child set to \c true for copying the source
     *                      as a child of the \c destPath if the name of
     *                      srcPathList and destPath are the same.
     * \param make_parents if true, any non-existent parent dirs are also created
     * \param ignoreExternals
     * \param revProps
     * \return TRUE if successful
     */
    bool Copy(const CTSVNPathList& srcPathList, const CTSVNPath& destPath,
        const SVNRev& revision, const SVNRev& pegrev, const CString& logmsg = CString(),
        bool copy_as_child = false, bool make_parents = false, bool ignoreExternals = false,
        bool pin_externals = false, SVNExternals externalsToTag = SVNExternals(),
        const RevPropHash& revProps = RevPropHash());
    /**
     * Move srcPath to destPath.
     *
     * srcPath must be a file or directory under version control and
     * destPath must also be a working copy path (existent or not).
     * This is a scheduling operation.  No changes will happen to the
     * repository until a commit occurs.  This scheduling can be removed
     * with Revert().  If srcPath is a file it is removed from
     * the working copy immediately.  If srcPath is a directory it will
     * remain in the working copy but all the files and unversioned items
     * it contains will be removed.
     *
     * If srcPath contains locally modified and/or unversioned items and
     * force is not set, the copy will fail. If force is set such items
     * will be removed.
     *
     * \param srcPathList source path list
     * \param destPath destination path
     * \param message the log message to use if the move is on an url
     * \param move_as_child set to \c true for moving the source
     *                      as a child of the \c destPath if the name of
     *                      srcPathList and destPath are the same.
     * \param make_parents if true, any non-existent parent dirs are also created
     * \param allow_mixed
     * \param metadata_only
     * \param revProps
     * \return TRUE if successful
     */
    bool Move(const CTSVNPathList& srcPathList, const CTSVNPath& destPath,
                const CString& message = L"", bool move_as_child = false,
                bool make_parents = false, bool allow_mixed = false, bool metadata_only = false, const RevPropHash& revProps = RevPropHash());
    /**
     * If path is a URL, use the message to immediately
     * attempt to commit the creation of the directory URL in the
     * repository.
     * Else, create the directory on disk, and attempt to schedule it for
     * addition.
     *
     * \param pathlist
     * \param message
     * \param makeParents create any non-existent parent directories also
     * \param revProps
     * \return TRUE if successful
     */
    bool MakeDir(const CTSVNPathList& pathlist, const CString& message, bool makeParents, const RevPropHash& revProps = RevPropHash());
    /**
     * Recursively cleanup a working copy directory DIR, finishing any
     * incomplete operations, removing lock files, etc.
     * \param path the file/directory to clean up
     * \return TRUE if successful
     */
    bool CleanUp(const CTSVNPath& path, bool breaklocks, bool fixtimestamps, bool cleardavcache, bool vacuumpristines, bool includeexternals);
    /**
     * Recursively vacuum a working copy directory dir, removing unnecessary data.
     * \param path the path to the working copy
     * \param unversioned if true, remove all unversioned items
     * \param ignored if true, remove all ignored items
     * \param fixtimestamps if true, fix all timestamps on files
     * \param pristines if true, remove unnecessary pristine copies
     * \param includeexternals if true, recurse into externals as well
     */
    bool Vacuum(const CTSVNPath& path, bool unversioned, bool ignored, bool fixtimestamps, bool pristines, bool includeExternals);
    /**
     * Remove the 'conflicted' state on a working copy PATH.  This will
     * not semantically resolve conflicts;  it just allows path to be
     * committed in the future.
     * If recurse is set, recurse below path, looking for conflicts to
     * resolve.
     * If path is not in a state of conflict to begin with, do nothing.
     *
     * \param path the path to resolve
     * \param result
     * \param recurse
     * \param typeonly if true, only the specified conflict \c kind is resolved
     * \param kind the conflict kind (text, property, tree) to resolve, only used if \c typeonly is true
     * \return TRUE if successful
     */
    bool Resolve(const CTSVNPath& path, svn_wc_conflict_choice_t result, bool recurse, bool typeonly, svn_wc_conflict_kind_t kind);
    /**
     * Export the contents of either a subversion repository or a subversion
     * working copy into a 'clean' directory (meaning a directory with no
     * administrative directories).

     * \param srcPath   either the path the working copy on disk, or a url to the
     *                  repository you wish to export.
     * \param destPath  the path to the directory where you wish to create the exported tree.
     * \param pegrev
     * \param revision  the revision that should be exported, which is only used
     *                  when exporting from a repository.
     * \param force     TRUE if existing files should be overwritten
     * \param bIgnoreExternals does not export externals if true
     * \param bIgnoreKeywords  does not expand svn:keywords during export - only applies when exporting from an url
     * \param depth     the export depth
     * \param hWnd      the handle of the parent window, used to show progress dialogs
     * \param extended  if true, unversioned items are exported too, only applies when exporting from a working copy
     * \param eol       "", "CR", "LF" or "CRLF" - "" being the default
     * \return TRUE if successful
     */
    bool Export(const CTSVNPath& srcPath, const CTSVNPath& destPath, const SVNRev& pegrev, const SVNRev& revision,
        bool force = true, bool bIgnoreExternals = false, bool bIgnoreKeywords = false, svn_depth_t depth = svn_depth_infinity,
        HWND hWnd = NULL, SVNExportType extended = SVNExportNormal, const CString& eol = CString());
    /**
     * Switch working tree path to URL at revision
     *
     * Summary of purpose: this is normally used to switch a working
     * directory over to another line of development, such as a branch or
     * a tag.  Switching an existing working directory is more efficient
     * than checking out URL from scratch.
     *
     * \param path the path of the working directory
     * \param url the url of the repository
     * \param revision the revision number to switch to
     * \param pegrev
     * \param depth the Subversion depth enum
     * \param depthIsSticky
     * \param ignore_externals
     * \param allow_unver_obstruction if true then the switch tolerates
     * existing unversioned items that obstruct added paths from @a path.  Only
     * obstructions of the same type (file or dir) as the added item are
     * tolerated.  The text of obstructing files is left as-is, effectively
     * treating it as a user modification after the switch.  Working
     * properties of obstructing items are set equal to the base properties.
     * If @a allow_unver_obstructions is false then the switch will abort
     * if there are any unversioned obstructing items.
     * \param ignore_ancestry
     * \return TRUE if successful
     */
    bool Switch(const CTSVNPath& path, const CTSVNPath& url, const SVNRev& revision,
        const SVNRev& pegrev, svn_depth_t depth, bool depthIsSticky,
        bool ignore_externals, bool allow_unver_obstruction, bool ignore_ancestry);
    /**
     * Import file or directory path into repository directory url at
     * head and using LOG_MSG as the log message for the (implied)
     * commit.
     * If path is a directory, the contents of that directory are
     * imported, under a new directory named newEntry under url; or if
     * newEntry is null, then the contents of path are imported directly
     * into the directory identified by url.  Note that the directory path
     * itself is not imported -- that is, the basename of path is not part
     * of the import.
     *
     * If path is a file, that file is imported as newEntry (which may
     * not be null).
     *
     * In all cases, if newEntry already exists in url, return error.
     *
     * \param path      the file/directory to import
     * \param url       the url to import to
     * \param message   log message used for the 'commit'
     * \param props
     * \param depth
     * \param bUseAutoprops
     * \param no_ignore If no_ignore is FALSE, don't add files or directories that match ignore patterns.
     * \param ignore_unknown
     * \param revProps
     * \return TRUE if successful
     */
    bool Import(const CTSVNPath& path, const CTSVNPath& url, const CString& message,
        ProjectProperties * props, svn_depth_t depth, bool bUseAutoprops, bool no_ignore, bool ignore_unknown, const RevPropHash& revProps = RevPropHash());
    /**
     * Merge changes from path1/revision1 to path2/revision2 into the
     * working-copy path localPath.  path1 and path2 can be either
     * working-copy paths or URLs.
     *
     * By "merging", we mean:  apply file differences and schedule
     * additions & deletions when appropriate.
     *
     * path1 and path2 must both represent the same node kind -- that is,
     * if path1 is a directory, path2 must also be, and if path1 is a
     * file, path2 must also be.
     *
     * If recurse is true (and the paths are directories), apply changes
     * recursively; otherwise, only apply changes in the current
     * directory.
     *
     * If force is not set and the merge involves deleting locally modified or
     * unversioned items the operation will fail.  If force is set such items
     * will be deleted.
     *
     * \param path1     first path
     * \param revision1 first revision
     * \param path2     second path
     * \param revision2 second revision
     * \param localPath destination path
     * \param force     see description
     * \param depth     the Subversion depth enum
     * \param options
     * \param ignoreancestry
     * \param dryrun
     * \param record_only   If record_only is true, the merge isn't actually performed,
     *                      but the merge info for the revisions which would've been
     *                      merged is recorded in the working copy (and must be subsequently
     *                      committed back to the repository).
     * \return TRUE if successful
     */
    bool Merge(const CTSVNPath& path1, const SVNRev& revision1, const CTSVNPath& path2,
        const SVNRev& revision2, const CTSVNPath& localPath, bool force, svn_depth_t depth, const CString& options,
        bool ignoreanchestry = FALSE, bool dryrun = FALSE, bool record_only = FALSE);

    /**
     * Merge changes from source/revision1 to source/revision2 into the
     * working-copy path localPath.
     *
     * The peg revision is used as an anchor where to look for \c source
     * since it may have been moved/renamed and does not exist anymore with the
     * name specified by \c source in \c revision1 or \c revision2.
     *
     * By "merging", we mean:  apply file differences and schedule
     * additions & deletions when appropriate.
     *
     * If recurse is true (and the paths are directories), apply changes
     * recursively; otherwise, only apply changes in the current
     * directory.
     *
     * If force is not set and the merge involves deleting locally modified or
     * unversioned items the operation will fail.  If force is set such items
     * will be deleted.
     *
     * \param source        url
     * \param revrangearray revisions
     * \param pegrevision   the peg revision
     * \param destpath      destination path
     * \param force         see description
     * \param depth         the Subversion depth enum
     * \param options
     * \param ignoreancestry
     * \param dryrun
     * \param record_only   If record_only is true, the merge isn't actually performed,
     *                      but the merge info for the revisions which would've been
     *                      merged is recorded in the working copy (and must be subsequently
     *                      committed back to the repository).
     * \return TRUE if successful
     */
    bool PegMerge(const CTSVNPath& source, const SVNRevRangeArray& revrangearray, const SVNRev& pegrevision,
        const CTSVNPath& destpath, bool force, svn_depth_t depth, const CString& options,
        bool ignoreancestry = FALSE, bool dryrun = FALSE, bool record_only = FALSE);
    /**
     * Performs a reintegration merge of \c source into \c wcpath.
     */
    bool MergeReintegrate(const CTSVNPath& source, const SVNRev& pegrevision,
        const CTSVNPath& wcpath, bool dryrun, const CString& options);
    /**
     * Returns a list of suggested source URLs for the given \c targetpath in \c revision.
     */
    bool SuggestMergeSources(const CTSVNPath& targetpath, const SVNRev& revision, CTSVNPathList& sourceURLs);
    /**
     * Produce diff output which describes the delta between \a path1/\a revision1 and \a path2/\a revision2
     * Print the output of the diff to \a outputfile, and any errors to \a errorfile. \a path1
     * and \a path2 can be either working-copy paths or URLs.
     * \a path1 and \a path2 must both represent the same node kind -- that
     * is, if \a path1 is a directory, \a path2 must also be, and if \a path1
     * is a file, \a path2 must also be.  (Currently, \a path1 and \a path2
     * must be the exact same path)
     *
     * Use \a ignore_ancestry to control whether or not items being
     * diffed will be checked for relatedness first.  Unrelated items
     * are typically transmitted to the editor as a deletion of one thing
     * and the addition of another, but if this flag is \c TRUE,
     * unrelated items will be diffed as if they were related.
     *
     * If \a no_diff_deleted is true, then no diff output will be
     * generated on deleted files.
     *
     * \a diff_options (an array of <tt>const char *</tt>) is used to pass
     * additional command line options to the diff processes invoked to compare files.
     *
     * \remark - the use of two overloaded functions rather than default parameters is to avoid the
     * CTSVNPath constructor (and hence #include) being visible in this header file
     * \return TRUE if successful
     */
    bool Diff(const CTSVNPath& path1, const SVNRev& revision1,
        const CTSVNPath& path2, const SVNRev& revision2,
        const CTSVNPath& relativeToDir, svn_depth_t depth,
        bool ignoreancestry, bool nodiffadded, bool nodiffdeleted, bool showCopiesAsAdds, bool ignorecontenttype, bool useGitFormat,
        bool ignoreproperties, bool propertiesonly,
        const CString& options, bool bAppend, const CTSVNPath& outputfile, const CTSVNPath& errorfile);
    bool Diff(const CTSVNPath& path1, const SVNRev& revision1,
        const CTSVNPath& path2, const SVNRev& revision2,
        const CTSVNPath& relativeToDir, svn_depth_t depth, bool ignoreancestry,
        bool nodiffadded, bool nodiffdeleted, bool showCopiesAsAdds, bool ignorecontenttype, bool useGitFormat,
        bool ignoreproperties, bool propertiesonly, const CString& options,
        bool bAppend, const CTSVNPath& outputfile);
    bool CreatePatch(const CTSVNPath& path1, const SVNRev& revision1,
        const CTSVNPath& path2, const SVNRev& revision2,
        const CTSVNPath& relativeToDir, svn_depth_t depth, bool ignoreancestry,
        bool nodiffadded, bool nodiffdeleted, bool showCopiesAsAdds, bool ignorecontenttype, bool useGitFormat,
        bool ignoreproperties, bool propertiesonly, const CString& options, bool bAppend, const CTSVNPath& outputfile);

    /**
     * Produce diff output which describes the delta between the file system object \a path in
     * peg revision \a pegrevision, as it changed between \a startrev and \a endrev.
     * Print the output of the diff to outputfile, and any errors to errorfile.
     * \a path can be either a working-copy path or URL.
     *
     * All other options are handled identically to Diff().
     * \return TRUE if successful
     */
    bool PegDiff(const CTSVNPath& path, const SVNRev& pegrevision, const SVNRev& startrev,
        const SVNRev& endrev, const CTSVNPath& relativeToDir, svn_depth_t depth,
        bool ignoreancestry, bool nodiffadded, bool nodiffdeleted, bool showCopiesAsAdds, bool ignorecontenttype, bool useGitFormat,
        bool ignoreproperties, bool propertiesonly, const CString& options,
        bool bAppend, const CTSVNPath& outputfile, const CTSVNPath& errorfile);
    bool PegDiff(const CTSVNPath& path, const SVNRev& pegrevision, const SVNRev& startrev,
        const SVNRev& endrev, const CTSVNPath& relativeToDir, svn_depth_t depth,
        bool ignoreancestry, bool nodiffadded, bool nodiffdeleted, bool showCopiesAsAdds, bool ignorecontenttype, bool useGitFormat,
        bool ignoreproperties, bool propertiesonly, const CString& options,
        bool bAppend, const CTSVNPath& outputfile);

    /**
     * Finds out what files/folders have changed between two paths/revs,
     * without actually doing the whole diff.
     * The result is passed in the DiffSummarizeCallback() callback function.
     * \param path1 the first path/url
     * \param rev1 the revision of the first path/url
     * \param path2 the second path/url
     * \param rev2 the revision of the second path/url
     * \param depth the Subversion depth enum
     * \param ignoreancestry if true, then possible ancestry between path1 and path2 is ignored
     * \return TRUE if successful
     */
    bool DiffSummarize(const CTSVNPath& path1, const SVNRev& rev1, const CTSVNPath& path2, const SVNRev& rev2, svn_depth_t depth, bool ignoreancestry);

    /**
    * Same as DiffSummarize(), expect this method takes a peg revision
    * to anchor the path on.
    * \return TRUE if successful
    */
    bool DiffSummarizePeg(const CTSVNPath& path, const SVNRev& peg, const SVNRev& rev1, const SVNRev& rev2, svn_depth_t depth, bool ignoreancestry);

    /**
    * Find the log cache object that contains / will contain the log information
    * for the given path.
    * \param path the path / url to find the cache for
    * \return NULL if log caching is disabled
    */
    LogCache::CCachedLogInfo* GetLogCache (const CTSVNPath& path);

    /**
     * fires the Log-event on each log message from revisionStart
     * to revisionEnd inclusive (but never fires the event
     * on a given log message more than once).
     * To receive the messages you need to listen to Log() events.
     *
     * \param pathlist the file/directory to get the log of
     * \param revisionPeg the peg revision to anchor the log on
     * \param revisionStart the revision to start the logs from
     * \param revisionEnd the revision to stop the logs
     * \param limit number of log messages to fetch, or zero for all
     * \param strict if TRUE, then the log won't follow copies
     * \param withMerges TRUE if the log should contain merged revisions
     *        as children
     * \param refresh fetch data from repository even if log caching
     *        is enabled. Merge the result with the existing caches.
     *        Ignored if log caching has been disabled.
     * \return pointer to the query run (and thus the resulting data),
     *        if successful. NULL, otherwise.
     */
    std::unique_ptr<const CCacheLogQuery>
    ReceiveLog(const CTSVNPathList& pathlist, const SVNRev& revisionPeg, const SVNRev& revisionStart,
        const SVNRev& revisionEnd, int limit, bool strict, bool withMerges, bool refresh);

    /**
     * Checks out a file with \a revision to \a localpath.
     * \param url
     * \param pegrevision
     * \param revision the revision of the file to checkout
     * \param localpath the place to store the file
     * \return TRUE if successful
     */
    bool Cat(const CTSVNPath& url, const SVNRev& pegrevision, const SVNRev& revision, const CTSVNPath& localpath);

    /**
     * Report the directory entry, and possibly children, for \c url at \c revision.
     *
     * The actual node revision selected is determined by the path as it exists in \c pegrev.
     * If \c pegrev->kind is \c svn_opt_revision_unspecified, then it defaults to \c svn_opt_revision_head for urls
     * and svn_opt_revision_working for WC targets.
     *
     * Report directory entries by invoking the virtual method ReportList().
     *
     * If \c fetchlocks is true, include locks when reporting directory entries.
     * \return TRUE if successful
     */
    bool List(const CTSVNPath& url, const SVNRev& revision, const SVNRev& pegrev, svn_depth_t depth, bool fetchlocks, apr_uint32_t dirents, bool includeExternals);

    /**
     * Relocates a working copy to a new/changes repository URL. Use this function
     * if the URL of the repository server has changed.
     * \param path path to the working copy
     * \param from the old URL of the repository
     * \param to the new URL of the repository
     * \param includeexternals TRUE if the operation should also apply to externals
     * \return TRUE if successful
     */
    bool Relocate(const CTSVNPath& path, const CTSVNPath& from, const CTSVNPath& to, bool includeexternals);

    /**
     * Determine the author for each line in a file (blame the changes on someone).
     * The result is given in the callback function BlameCallback()
     *
     * \param path the path or url of the file
     * \param startrev the revision from which the check is done from
     * \param endrev the end revision where the check is stopped
     * \param peg the peg revision to use
     * \param diffoptions options for the internal diff to use when blaming
     * \param ignoremimetype set to true if you want to ignore the mimetype and blame everything
     * \param includemerge if true, also return data based upon revisions which have been merged to path.
     * \return TRUE if successful
     */
    bool Blame(const CTSVNPath& path, const SVNRev& startrev, const SVNRev& endrev,
        const SVNRev& peg, const CString& diffoptions, bool ignoremimetype = false, bool includemerge = true);

    /**
     * Lock a file for exclusive use so no other users are allowed to edit
     * the same file. A commit of a locked file is rejected if the lock isn't
     * owned by the committer himself.
     * \param pathList a list of filepaths to lock
     * \param bStealLock if TRUE, an already existing lock is overwritten
     * \param comment a comment to assign to the lock. Only used by svnserve!
     * \return TRUE if successful
     */
    bool Lock(const CTSVNPathList& pathList, bool bStealLock, const CString& comment = CString());

    /**
     * Removes existing locks from files.
     * \param pathList a list of file paths to remove the lock from
     * \param bBreakLock if TRUE, the locks are removed even if the committer
     * isn't the owner of the locks!
     * \return TRUE if successful
     */
    bool Unlock(const CTSVNPathList& pathList, bool bBreakLock);

    /**
     * Checks if a windows path is a local repository
     */
    bool IsRepository(const CTSVNPath& path);

    /**
     * Returns the repository UUID for the \c path.
     */
    CString GetUUIDFromPath(const CTSVNPath& path);
    /**
     * Finds the repository root of a given url or path.
     * \return The root url or an empty string
     */
    CString GetRepositoryRoot(const CTSVNPath& url);
    /**
     * Finds the repository root of a given url, and the UUID of the repository.
     * \param path [in] the WC path / URL to get the root and UUID from
     * \param useLogCache [in] if set, try to use the log cache first
     *                         (if log caching has been enabled at all)
     * \param sUUID [out] the UUID of the repository
     * \return the root url or an empty string
     */
    CString GetRepositoryRootAndUUID(const CTSVNPath& path, bool useLogCache, CString& sUUID);

    /**
     * Returns the HEAD revision of the URL or WC-Path.
     * Set @a cacheAllowed to @n false to force repository access.
     * Or -1 if the function failed.
     */
    svn_revnum_t GetHEADRevision(const CTSVNPath& path, bool cacheAllowed = true);

    /**
     * Returns the revision the last commit/add/mkdir/... command created
     */
    svn_revnum_t GetCommitRevision() const { return m_commitRev; }

    /**
     * Returns the repository root and the HEAD revision of the repository.
     */
    bool GetRootAndHead(const CTSVNPath& path, CTSVNPath& url, svn_revnum_t& rev);

    /**
     * Set the revision property \a sName to the new value \a sValue.
     * \param sName property name to set
     * \param sValue the value for the new property
     * \param sOldValue the value the property had before, or an empty string if not known
     * \param URL the URL of the file/folder
     * \param rev the revision number to change the revprop
     * \return the actual revision number the property value was set
     */
    svn_revnum_t RevPropertySet(const CString& sName, const CString& sValue, const CString& sOldValue, const CTSVNPath& URL, const SVNRev& rev);

    /**
     * Reads the revision property \a sName and returns its value.
     * \param sName property name to read
     * \param URL the URL of the file/folder
     * \param rev the revision number
     * \return the value of the property
     */
    CString RevPropertyGet(const CString& sName, const CTSVNPath& URL, const SVNRev& rev);

    /**
     * Fetches all locks for \a url and the paths below.
     * \remark The CString key in the map is the absolute path in
     * the repository of the lock. It is \b not an absolute URL, the
     * repository root part is stripped off!
     */
    bool GetLocks(const CTSVNPath& url, std::map<CString, SVNLock> * locks);

    /**
     * get a summary of the working copy revisions
     * \param wcpath the path to the working copy to scan for
     * \param bCommitted if true, use the last-committed revisions, if false use the BASE revisions.
     * \param minrev the min revision of the working copy
     * \param maxrev the max revision of the working copy
     * \param switched true if one or more items in the working copy are switched
     * \param modified true if there are modified files in the working copy
     * \param sparse true if the entry is not of depth svn_depth_infinity
     */
    bool GetWCRevisionStatus(const CTSVNPath& wcpath, bool bCommitted, svn_revnum_t& minrev, svn_revnum_t& maxrev, bool& switched, bool& modified, bool& sparse);

    /**
     * Set \a minrev and \a maxrev to the lowest and highest revision numbers found within \a wcpath.
     * if \a committed is set to true, set \a minrev and \a maxrev to
     * the lowest and highest committed (i.e. "last changed") revision numbers.
     */
    bool GetWCMinMaxRevs(const CTSVNPath& wcpath, bool committed, svn_revnum_t& minrev, svn_revnum_t& maxrev);

    /**
     * Upgrades the working copy at \c wcpath to the new format
     */
    bool Upgrade(const CTSVNPath& wcpath);

    /**
     * Returns the URL associated with the \c path.
     */
    CString GetURLFromPath(const CTSVNPath& path);
    /**
     * Returns the wc root of the specified \c path.
     */
    CTSVNPath GetWCRootFromPath(const CTSVNPath& path);

    /**
     * Returns the checksum of the passed string.
     * type is either svn_checksum_md5 or svn_checksum_sha1.
     */
    CString GetChecksumString(svn_checksum_kind_t type, const CString& s);
    static CString GetChecksumString(svn_checksum_kind_t type, const CString& s, apr_pool_t * localpool);

    /**
     * Creates a repository at the specified location.
     * \param path where the repository should be created
     * \param fstype repository file system type. Default is fsfs.
     * \return TRUE if operation was successful
     */
    static bool CreateRepository(const CTSVNPath& path, const CString& fstype = L"fsfs");

    /**
     * Convert Windows Path to Local Repository URL
     */
    static void PathToUrl(CString &path);

    /**
     * Convert Local Repository URL to Windows Path
     */
    static void UrlToPath(CString &url);

    /**
     * Returns the path to the text-base file of the working copy file.
     * If no text base exists for the file then the returned string is empty.
     */
    static CTSVNPath GetPristinePath(const CTSVNPath& wcPath);

    /**
     * convert path to a subversion path (replace '\' with '/')
     */
    static void preparePath(CString &path);

    /**
     * Checks if a given path is a valid URL.
     */
    static bool PathIsURL(const CTSVNPath& path);

    /**
     * Creates the Subversion config file if it doesn't already exist.
     */
    static bool EnsureConfigFile();

    /**
     * Returns the status of the encapsulated \ref SVNPrompt instance.
     */
    bool PromptShown() const;
    /**
     * Set the parent window of an authentication prompt dialog
     */
    void SetPromptParentWindow(HWND hWnd);
    /**
     * Set the MFC Application object for a prompt dialog
     */
    void SetPromptApp(CWinApp* pWinApp);

    /**
     * Sets and clears the progress info which is shown during lengthy operations.
     * \param hWnd the window handle of the parent dialog/window
     */
    void SetAndClearProgressInfo(HWND hWnd);
    /**
     * Sets and clears the progress info which is shown during lengthy operations.
     * \param pProgressDlg the CProgressDlg object to show the progress info on.
     * \param bShowProgressBar set to true if the progress bar should be shown. Only makes
     * sense if the total amount of the progress is known beforehand. Otherwise the
     * progressbar is always "empty".
     */
    void SetAndClearProgressInfo(CProgressDlg * pProgressDlg, bool bShowProgressBar = false);

    /**
     * Returns the string representation of the summarize action.
     */
    static CString GetSummarizeActionText(svn_client_diff_summarize_kind_t kind);

    /**
     * Converts a Subversion url/path to an UI format (utf16 encoded, unescaped,
     * backslash for paths, forward slashes for urls).
     */
    static CString MakeUIUrlOrPath(const CStringA& UrlOrPath);

    /**
     * Converts a Subversion url/path to an UTF8-encoded UI format (unescaped,
     * backslash for paths, forward slashes for urls).
     */
    static std::string MakeUIUrlOrPath(const char* UrlOrPath);
    /**
     * Returns a string in \c date_native[] representing the date in the OS local
     * format.
     */
    static void formatDate(TCHAR date_native[], apr_time_t date_svn, bool force_short_fmt = false);
    /**
     * Returns a string in \c date_native[] representing the date in the OS local
     * format.
     */
    static void formatDate(TCHAR date_native[], FILETIME& date, bool force_short_fmt = false);
    /**
     * Returns a string representing the date w/o time in the OS local format.
     */
    static CString formatDate (apr_time_t date_svn);
    /**
     * Returns a string representing the time w/o date in the OS local format.
     */
    static CString formatTime (apr_time_t date_svn);

    /**
     * Returns a string which can be passed as the options string for the Merge()
     * methods and the Blame() method.
     */
    static CString GetOptionsString(bool bIgnoreEOL, bool bIgnoreSpaces, bool bIgnoreAllSpaces);
    static CString GetOptionsString(bool bIgnoreEOL, svn_diff_file_ignore_space_t space = svn_diff_file_ignore_space_none);

    /**
     * Returns the log cache pool singleton. You will need that to
     * create \c CCacheLogQuery instances.
     */
    LogCache::CLogCachePool* GetLogCachePool();

    void SetCancelBool(bool * bCancel) { m_pbCancel = bCancel; }

    const CTSVNPath& GetRedirectedUrlPath() const {return m_redirectedUrl;}

    /// override the cache enabled setting and assume the cache is enabled
    void AssumeCacheEnabled(bool bEnable) { m_bAssumeCacheEnabled = bEnable; }

    /// suppresses all UI dialogs like authentication dialogs
    void SuppressUI(bool bSuppress) { m_prompt.SuppressUI(bSuppress); }
    bool IsSuppressedUI() { return m_prompt.IsSilent(); }
    void SetAuthInfo(const CString& username, const CString& password);

    /**
     * Reinitializes the client context structure m_pctx.
     * This is required to get rid of file locks on wc.db files because
     * svn uses the pool used to create the client context to open the SQLite
     * database files. And those files are only closed when the pool is cleared!
     */
    void SVNReInit();

    /**
     * Resolves tree conflict.
     */
    bool ResolveTreeConflict(svn_client_conflict_t *conflict, svn_client_conflict_option_t *option);
    /**
     * Resolves text conflict.
     */
    bool ResolveTextConflict(svn_client_conflict_t * conflict, svn_client_conflict_option_t * option);

    /**
     * Resolves property conflict.
     */
    bool ResolvePropConflict(svn_client_conflict_t * conflict, const CString & propName, svn_client_conflict_option_t * option);

protected:
    apr_pool_t *                parentpool;     ///< the main memory pool
    apr_pool_t *                m_pool;         ///< 'root' memory pool
    SVNPrompt                   m_prompt;
    svn_revnum_t                m_commitRev;    ///< revision of the last commit/add/mkdir
    bool *                      m_pbCancel;
    CTSVNPath                   m_redirectedUrl;///< the target url in case of a redirect
    svn_wc_conflict_kind_t      m_resolvekind;  ///< resolve kind for the conflict resolver callback
    svn_wc_conflict_choice_t    m_resolveresult;///< resolve result for the conflict resolver callback
    bool                        m_bAssumeCacheEnabled;  ///< if true, overrides the log cache enabled setting and assumes it is enabled
    static LCID                 s_locale;
    static bool                 s_useSystemLocale;

    void * logMessage (CString message, char * baseDirectory = NULL);

    /// Convert a TSVNPathList into an array of SVN copy paths
    apr_array_header_t * MakeCopyArray(const CTSVNPathList& pathList, const SVNRev& rev, const SVNRev& pegrev);
    apr_array_header_t * MakeChangeListArray(const CStringArray& changelists, apr_pool_t * pool);
    apr_hash_t *         MakeRevPropHash(const RevPropHash& revProps, apr_pool_t * pool);

    void                 CallPreConnectHookIfUrl(const CTSVNPathList& pathList, const CTSVNPath& path = CTSVNPath());
    void                 Prepare();
    void                 SVNInit();
    static bool          AprTimeExplodeLocal(apr_time_exp_t *exploded_time, apr_time_t date_svn);
    bool                 IsLogCacheEnabled();

    void cancel();
    static svn_error_t* cancel(void *baton);
    static svn_error_t * commitcallback2(const svn_commit_info_t * commit_info, void * baton, apr_pool_t * localpool);
    static void notify( void *baton,
                        const svn_wc_notify_t *notify,
                        apr_pool_t *pool);
    static svn_error_t* summarize_func(const svn_client_diff_summarize_t *diff,
                    void *baton, apr_pool_t *pool);
    static svn_error_t* blameReceiver(void *baton,
                                      svn_revnum_t start_revnum,
                                      svn_revnum_t end_revnum,
                                      apr_int64_t line_no,
                                      svn_revnum_t revision,
                                      apr_hash_t *rev_props,
                                      svn_revnum_t merged_revision,
                                      apr_hash_t *merged_rev_props,
                                      const char *merged_path,
                                      const char *line,
                                      svn_boolean_t local_change,
                    apr_pool_t *pool);
    static svn_error_t* listReceiver(void* baton,
                    const char* path,
                    const svn_dirent_t *dirent,
                    const svn_lock_t *lock,
                    const char *abs_path,
                    const char *external_parent_url,
                    const char *external_target,
                    apr_pool_t *pool);
    static svn_error_t* conflict_resolver(svn_wc_conflict_result_t **result,
                    const svn_wc_conflict_description2_t *description,
                    void *baton,
                    apr_pool_t * resultpool,
                    apr_pool_t * scratchpool);

    static svn_error_t * import_filter(void *baton,
                                       svn_boolean_t *filtered,
                                       const char *local_abspath,
                                       const svn_io_dirent2_t *dirent,
                                       apr_pool_t *scratch_pool);


    // implement ILogReceiver

    void ReceiveLog ( TChangedPaths* changes
                    , svn_revnum_t rev
                    , const StandardRevProps* stdRevProps
                    , UserRevPropArray* userRevProps
                    , const MergeInfo* mergeInfo) override;

    // logCachePool management utilities

    async::CCriticalSection& GetLogCachePoolMutex();
    void ResetLogCachePool();

    static void progress_func(apr_off_t progress, apr_off_t total, void *baton, apr_pool_t *pool);
    SVNProgress     m_SVNProgressMSG;
    HWND            m_progressWnd;
    CProgressDlg *  m_pProgressDlg;
    bool            m_progressWndIsCProgress;
    bool            m_bShowProgressBar;
    apr_off_t   progress_total;
    apr_off_t   progress_averagehelper;
    apr_off_t   progress_lastprogress;
    apr_off_t   progress_lasttotal;
    ULONGLONG   progress_lastTicks;
    std::vector<apr_off_t> progress_vector;

private:

    // the logCachePool must not be static: it uses apr/svn pools, and static objects
    // will get cleaned up *after* we shut down apr.
    std::unique_ptr<LogCache::CLogCachePool> logCachePool;
};

static UINT WM_SVNPROGRESS = RegisterWindowMessage(L"TORTOISESVN_SVNPROGRESS_MSG");

void AprTimeToFileTime(LPFILETIME pft, apr_time_t t);
