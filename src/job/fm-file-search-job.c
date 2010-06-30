#include "fm-file-search-job.h"

#include "fm-folder.h"
#include "fm-list.h"
#include <string.h> /* for strstr */

static void fm_file_search_job_finalize(GObject * object);
gboolean fm_file_search_job_run(FmFileSearchJob* job);

G_DEFINE_TYPE(FmFileSearchJob, fm_file_search_job, FM_TYPE_JOB);

static void for_each_target_folder(FmPath * path, FmFileSearchJob * job);
static void on_target_folder_file_list_loaded(FmFileInfoList * list, FmFileSearchJob * job);
static void for_each_file_info(FmFileInfo * info, FmFileSearchJob * job);

static void fm_file_search_job_class_init(FmFileSearchJobClass * klass)
{
	GObjectClass * g_object_class;
	FmJobClass * job_class = FM_JOB_CLASS(klass);
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_file_search_job_finalize;

	job_class->run = fm_file_search_job_run;
}


static void fm_file_search_job_init(FmFileSearchJob * self)
{
    fm_job_init_cancellable(FM_JOB(self));
}

static void fm_file_search_job_finalize(GObject * object)
{
	FmFileSearchJob *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(FM_IS_FILE_SEARCH_JOB(object));

	self = FM_FILE_SEARCH_JOB(object);

	if(self->files)
		fm_list_unref(self->files);

	if(self->target)
		g_free(self->target);
		
	if(self->target_folders)
		g_slist_free(self->target_folders);

	if(self->target_type)
		fm_mime_type_unref(self->target_type);

	if (G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)
		(* G_OBJECT_CLASS(fm_file_search_job_parent_class)->finalize)(object);
}

FmJob * fm_file_search_job_new(FmFileSearch * search)
{
	FmFileSearchJob * job = (FmJob*)g_object_new(FM_TYPE_FILE_SEARCH_JOB, NULL);

	job->files = fm_file_info_list_new();
	job->target = g_strdup(search->target);
	job->target_folders = g_slist_copy(search->target_folders);
	job->target_type = search->target_type; /* does this need to be referenced */
	job->check_type = search->check_type;

	return (FmJob*)job;
}

gboolean fm_file_search_job_run(FmFileSearchJob * job)
{
	/* TODO: error handling (what sort of errors could occur?) */	

	GSList * folders = job->target_folders;

	while(folders != NULL)
	{
		FmPath * path = FM_PATH(folders->data);
		for_each_target_folder(path, job);
		folders = folders->next;
	}
	
	return TRUE;
}

static void for_each_target_folder(FmPath * path, FmFileSearchJob * job)
{
	/* TODO: handle if the job is canceled since this is called by a for each */

	/* TODO: free job when finished using it */
	FmJob * file_list_job = fm_dir_list_job_new(path);
	fm_job_run_sync(file_list_job);
	on_target_folder_file_list_loaded(fm_dir_dist_job_get_files(file_list_job), job);
	/* else emit error */
}

static void on_target_folder_file_list_loaded(FmFileInfoList * info_list, FmFileSearchJob * job)
{
	GQueue queue = info_list->list;
	GList * file_info = g_queue_peek_head_link(&queue);

	while(file_info != NULL)
	{
		FmFileInfo * info = FM_FILE_INFO(file_info->data);
		for_each_file_info(info, job);
		file_info = file_info->next;
	}
}

static void for_each_file_info(FmFileInfo * info, FmFileSearchJob * job)
{
	
	/* TODO: handle if the job is canceled since this is called by a for each */

	if(strstr(fm_file_info_get_name(info), job->target) != NULL)
	{
		//printf("%s\n", fm_file_info_get_name(info));
		fm_list_push_tail_noref(job->files, info); /* should be referenced because the file list will be freed ? */
	}

	/* TODO: checking that mime matches */
	/* TODO: checking contents of files */
	/* TODO: use mime type when possible to prevent the unneeded checking of file contents */

	/* recurse upon each directory */
	if(fm_file_info_is_dir(info))
		for_each_target_folder(fm_file_info_get_path(info),job);
}

FmFileInfoList* fm_file_search_job_get_files(FmFileSearchJob* job)
{
	return job->files;
}
