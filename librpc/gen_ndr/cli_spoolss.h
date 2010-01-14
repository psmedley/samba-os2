#include "../librpc/gen_ndr/ndr_spoolss.h"
#ifndef __CLI_SPOOLSS__
#define __CLI_SPOOLSS__
NTSTATUS rpccli_spoolss_EnumPrinters(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     uint32_t flags /* [in]  */,
				     const char *server /* [in] [unique,charset(UTF16)] */,
				     uint32_t level /* [in]  */,
				     DATA_BLOB *buffer /* [in] [unique] */,
				     uint32_t offered /* [in]  */,
				     uint32_t *count /* [out] [ref] */,
				     union spoolss_PrinterInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				     uint32_t *needed /* [out] [ref] */,
				     WERROR *werror);
NTSTATUS rpccli_spoolss_OpenPrinter(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    const char *printername /* [in] [unique,charset(UTF16)] */,
				    const char *datatype /* [in] [unique,charset(UTF16)] */,
				    struct spoolss_DevmodeContainer devmode_ctr /* [in]  */,
				    uint32_t access_mask /* [in]  */,
				    struct policy_handle *handle /* [out] [ref] */,
				    WERROR *werror);
NTSTATUS rpccli_spoolss_SetJob(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       uint32_t job_id /* [in]  */,
			       struct spoolss_JobInfoContainer *ctr /* [in] [unique] */,
			       enum spoolss_JobControl command /* [in]  */,
			       WERROR *werror);
NTSTATUS rpccli_spoolss_GetJob(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       uint32_t job_id /* [in]  */,
			       uint32_t level /* [in]  */,
			       DATA_BLOB *buffer /* [in] [unique] */,
			       uint32_t offered /* [in]  */,
			       union spoolss_JobInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
			       uint32_t *needed /* [out] [ref] */,
			       WERROR *werror);
NTSTATUS rpccli_spoolss_EnumJobs(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in] [ref] */,
				 uint32_t firstjob /* [in]  */,
				 uint32_t numjobs /* [in]  */,
				 uint32_t level /* [in]  */,
				 DATA_BLOB *buffer /* [in] [unique] */,
				 uint32_t offered /* [in]  */,
				 uint32_t *count /* [out] [ref] */,
				 union spoolss_JobInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				 uint32_t *needed /* [out] [ref] */,
				 WERROR *werror);
NTSTATUS rpccli_spoolss_AddPrinter(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinter(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *handle /* [in] [ref] */,
				      WERROR *werror);
NTSTATUS rpccli_spoolss_SetPrinter(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   struct spoolss_SetPrinterInfoCtr *info_ctr /* [in] [ref] */,
				   struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
				   struct sec_desc_buf *secdesc_ctr /* [in] [ref] */,
				   enum spoolss_PrinterControl command /* [in]  */,
				   WERROR *werror);
NTSTATUS rpccli_spoolss_GetPrinter(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   uint32_t level /* [in]  */,
				   DATA_BLOB *buffer /* [in] [unique] */,
				   uint32_t offered /* [in]  */,
				   union spoolss_PrinterInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
				   uint32_t *needed /* [out] [ref] */,
				   WERROR *werror);
NTSTATUS rpccli_spoolss_AddPrinterDriver(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 const char *servername /* [in] [unique,charset(UTF16)] */,
					 struct spoolss_AddDriverInfoCtr *info_ctr /* [in] [ref] */,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_EnumPrinterDrivers(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   const char *server /* [in] [unique,charset(UTF16)] */,
					   const char *environment /* [in] [unique,charset(UTF16)] */,
					   uint32_t level /* [in]  */,
					   DATA_BLOB *buffer /* [in] [unique] */,
					   uint32_t offered /* [in]  */,
					   uint32_t *count /* [out] [ref] */,
					   union spoolss_DriverInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
					   uint32_t *needed /* [out] [ref] */,
					   WERROR *werror);
NTSTATUS rpccli_spoolss_GetPrinterDriver(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_GetPrinterDriverDirectory(struct rpc_pipe_client *cli,
						  TALLOC_CTX *mem_ctx,
						  const char *server /* [in] [unique,charset(UTF16)] */,
						  const char *environment /* [in] [unique,charset(UTF16)] */,
						  uint32_t level /* [in]  */,
						  DATA_BLOB *buffer /* [in] [unique] */,
						  uint32_t offered /* [in]  */,
						  union spoolss_DriverDirectoryInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
						  uint32_t *needed /* [out] [ref] */,
						  WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinterDriver(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    const char *server /* [in] [unique,charset(UTF16)] */,
					    const char *architecture /* [in] [charset(UTF16)] */,
					    const char *driver /* [in] [charset(UTF16)] */,
					    WERROR *werror);
NTSTATUS rpccli_spoolss_AddPrintProcessor(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  const char *server /* [in] [unique,charset(UTF16)] */,
					  const char *architecture /* [in] [charset(UTF16)] */,
					  const char *path_name /* [in] [charset(UTF16)] */,
					  const char *print_processor_name /* [in] [charset(UTF16)] */,
					  WERROR *werror);
NTSTATUS rpccli_spoolss_EnumPrintProcessors(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    const char *servername /* [in] [unique,charset(UTF16)] */,
					    const char *environment /* [in] [unique,charset(UTF16)] */,
					    uint32_t level /* [in]  */,
					    DATA_BLOB *buffer /* [in] [unique] */,
					    uint32_t offered /* [in]  */,
					    uint32_t *count /* [out] [ref] */,
					    union spoolss_PrintProcessorInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
					    uint32_t *needed /* [out] [ref] */,
					    WERROR *werror);
NTSTATUS rpccli_spoolss_GetPrintProcessorDirectory(struct rpc_pipe_client *cli,
						   TALLOC_CTX *mem_ctx,
						   const char *server /* [in] [unique,charset(UTF16)] */,
						   const char *environment /* [in] [unique,charset(UTF16)] */,
						   uint32_t level /* [in]  */,
						   DATA_BLOB *buffer /* [in] [unique] */,
						   uint32_t offered /* [in]  */,
						   union spoolss_PrintProcessorDirectoryInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
						   uint32_t *needed /* [out] [ref] */,
						   WERROR *werror);
NTSTATUS rpccli_spoolss_StartDocPrinter(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					uint32_t level /* [in]  */,
					union spoolss_DocumentInfo info /* [in] [switch_is(level)] */,
					uint32_t *job_id /* [out] [ref] */,
					WERROR *werror);
NTSTATUS rpccli_spoolss_StartPagePrinter(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_WritePrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     DATA_BLOB data /* [in]  */,
				     uint32_t _data_size /* [in] [value(r->in.data.length)] */,
				     uint32_t *num_written /* [out] [ref] */,
				     WERROR *werror);
NTSTATUS rpccli_spoolss_EndPagePrinter(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       WERROR *werror);
NTSTATUS rpccli_spoolss_AbortPrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     WERROR *werror);
NTSTATUS rpccli_spoolss_ReadPrinter(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *handle /* [in] [ref] */,
				    uint8_t *data /* [out] [ref,size_is(data_size)] */,
				    uint32_t data_size /* [in]  */,
				    uint32_t *_data_size /* [out] [ref] */,
				    WERROR *werror);
NTSTATUS rpccli_spoolss_EndDocPrinter(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *handle /* [in] [ref] */,
				      WERROR *werror);
NTSTATUS rpccli_spoolss_AddJob(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       uint32_t level /* [in]  */,
			       uint8_t *buffer /* [in,out] [unique,size_is(offered)] */,
			       uint32_t offered /* [in]  */,
			       uint32_t *needed /* [out] [ref] */,
			       WERROR *werror);
NTSTATUS rpccli_spoolss_ScheduleJob(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *handle /* [in] [ref] */,
				    uint32_t jobid /* [in]  */,
				    WERROR *werror);
NTSTATUS rpccli_spoolss_GetPrinterData(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       const char *value_name /* [in] [charset(UTF16)] */,
				       uint32_t offered /* [in]  */,
				       enum winreg_Type *type /* [out] [ref] */,
				       union spoolss_PrinterData *data /* [out] [subcontext_size(offered),ref,subcontext(4),switch_is(*type)] */,
				       uint32_t *needed /* [out] [ref] */,
				       WERROR *werror);
NTSTATUS rpccli_spoolss_SetPrinterData(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       const char *value_name /* [in] [charset(UTF16)] */,
				       enum winreg_Type type /* [in]  */,
				       union spoolss_PrinterData data /* [in] [subcontext(4),switch_is(type)] */,
				       uint32_t _offered /* [in] [value(ndr_size_spoolss_PrinterData(&data,type,ndr->iconv_convenience,flags))] */,
				       WERROR *werror);
NTSTATUS rpccli_spoolss_WaitForPrinterChange(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
NTSTATUS rpccli_spoolss_ClosePrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in,out] [ref] */,
				     WERROR *werror);
NTSTATUS rpccli_spoolss_AddForm(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				uint32_t level /* [in]  */,
				union spoolss_AddFormInfo info /* [in] [switch_is(level)] */,
				WERROR *werror);
NTSTATUS rpccli_spoolss_DeleteForm(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   const char *form_name /* [in] [charset(UTF16)] */,
				   WERROR *werror);
NTSTATUS rpccli_spoolss_GetForm(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				const char *form_name /* [in] [charset(UTF16)] */,
				uint32_t level /* [in]  */,
				DATA_BLOB *buffer /* [in] [unique] */,
				uint32_t offered /* [in]  */,
				union spoolss_FormInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
				uint32_t *needed /* [out] [ref] */,
				WERROR *werror);
NTSTATUS rpccli_spoolss_SetForm(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				const char *form_name /* [in] [charset(UTF16)] */,
				uint32_t level /* [in]  */,
				union spoolss_AddFormInfo info /* [in] [switch_is(level)] */,
				WERROR *werror);
NTSTATUS rpccli_spoolss_EnumForms(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *handle /* [in] [ref] */,
				  uint32_t level /* [in]  */,
				  DATA_BLOB *buffer /* [in] [unique] */,
				  uint32_t offered /* [in]  */,
				  uint32_t *count /* [out] [ref] */,
				  union spoolss_FormInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				  uint32_t *needed /* [out] [ref] */,
				  WERROR *werror);
NTSTATUS rpccli_spoolss_EnumPorts(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  const char *servername /* [in] [unique,charset(UTF16)] */,
				  uint32_t level /* [in]  */,
				  DATA_BLOB *buffer /* [in] [unique] */,
				  uint32_t offered /* [in]  */,
				  uint32_t *count /* [out] [ref] */,
				  union spoolss_PortInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				  uint32_t *needed /* [out] [ref] */,
				  WERROR *werror);
NTSTATUS rpccli_spoolss_EnumMonitors(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *servername /* [in] [unique,charset(UTF16)] */,
				     uint32_t level /* [in]  */,
				     DATA_BLOB *buffer /* [in] [unique] */,
				     uint32_t offered /* [in]  */,
				     uint32_t *count /* [out] [ref] */,
				     union spoolss_MonitorInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				     uint32_t *needed /* [out] [ref] */,
				     WERROR *werror);
NTSTATUS rpccli_spoolss_AddPort(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				const char *server_name /* [in] [unique,charset(UTF16)] */,
				uint32_t unknown /* [in]  */,
				const char *monitor_name /* [in] [charset(UTF16)] */,
				WERROR *werror);
NTSTATUS rpccli_spoolss_ConfigurePort(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePort(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
NTSTATUS rpccli_spoolss_CreatePrinterIC(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					WERROR *werror);
NTSTATUS rpccli_spoolss_PlayGDIScriptOnPrinterIC(struct rpc_pipe_client *cli,
						 TALLOC_CTX *mem_ctx,
						 WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinterIC(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					WERROR *werror);
NTSTATUS rpccli_spoolss_AddPrinterConnection(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinterConnection(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx,
						WERROR *werror);
NTSTATUS rpccli_spoolss_PrinterMessageBox(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  WERROR *werror);
NTSTATUS rpccli_spoolss_AddMonitor(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
NTSTATUS rpccli_spoolss_DeleteMonitor(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrintProcessor(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
NTSTATUS rpccli_spoolss_AddPrintProvidor(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrintProvidor(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    WERROR *werror);
NTSTATUS rpccli_spoolss_EnumPrintProcDataTypes(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       const char *servername /* [in] [unique,charset(UTF16)] */,
					       const char *print_processor_name /* [in] [unique,charset(UTF16)] */,
					       uint32_t level /* [in]  */,
					       DATA_BLOB *buffer /* [in] [unique] */,
					       uint32_t offered /* [in]  */,
					       uint32_t *count /* [out] [ref] */,
					       union spoolss_PrintProcDataTypesInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
					       uint32_t *needed /* [out] [ref] */,
					       WERROR *werror);
NTSTATUS rpccli_spoolss_ResetPrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     const char *data_type /* [in] [unique,charset(UTF16)] */,
				     struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
				     WERROR *werror);
NTSTATUS rpccli_spoolss_GetPrinterDriver2(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in] [ref] */,
					  const char *architecture /* [in] [unique,charset(UTF16)] */,
					  uint32_t level /* [in]  */,
					  DATA_BLOB *buffer /* [in] [unique] */,
					  uint32_t offered /* [in]  */,
					  uint32_t client_major_version /* [in]  */,
					  uint32_t client_minor_version /* [in]  */,
					  union spoolss_DriverInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
					  uint32_t *needed /* [out] [ref] */,
					  uint32_t *server_major_version /* [out] [ref] */,
					  uint32_t *server_minor_version /* [out] [ref] */,
					  WERROR *werror);
NTSTATUS rpccli_spoolss_FindFirstPrinterChangeNotification(struct rpc_pipe_client *cli,
							   TALLOC_CTX *mem_ctx,
							   WERROR *werror);
NTSTATUS rpccli_spoolss_FindNextPrinterChangeNotification(struct rpc_pipe_client *cli,
							  TALLOC_CTX *mem_ctx,
							  WERROR *werror);
NTSTATUS rpccli_spoolss_FindClosePrinterNotify(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *handle /* [in] [ref] */,
					       WERROR *werror);
NTSTATUS rpccli_spoolss_RouterFindFirstPrinterChangeNotificationOld(struct rpc_pipe_client *cli,
								    TALLOC_CTX *mem_ctx,
								    WERROR *werror);
NTSTATUS rpccli_spoolss_ReplyOpenPrinter(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 const char *server_name /* [in] [charset(UTF16)] */,
					 uint32_t printer_local /* [in]  */,
					 enum winreg_Type type /* [in]  */,
					 uint32_t bufsize /* [in] [range(0,512)] */,
					 uint8_t *buffer /* [in] [unique,size_is(bufsize)] */,
					 struct policy_handle *handle /* [out] [ref] */,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_RouterReplyPrinter(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *handle /* [in] [ref] */,
					   uint32_t flags /* [in]  */,
					   uint32_t bufsize /* [in] [range(0,512)] */,
					   uint8_t *buffer /* [in] [unique,size_is(bufsize)] */,
					   WERROR *werror);
NTSTATUS rpccli_spoolss_ReplyClosePrinter(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in,out] [ref] */,
					  WERROR *werror);
NTSTATUS rpccli_spoolss_AddPortEx(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  WERROR *werror);
NTSTATUS rpccli_spoolss_RouterFindFirstPrinterChangeNotification(struct rpc_pipe_client *cli,
								 TALLOC_CTX *mem_ctx,
								 WERROR *werror);
NTSTATUS rpccli_spoolss_SpoolerInit(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    WERROR *werror);
NTSTATUS rpccli_spoolss_ResetPrinterEx(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       WERROR *werror);
NTSTATUS rpccli_spoolss_RemoteFindFirstPrinterChangeNotifyEx(struct rpc_pipe_client *cli,
							     TALLOC_CTX *mem_ctx,
							     struct policy_handle *handle /* [in] [ref] */,
							     uint32_t flags /* [in]  */,
							     uint32_t options /* [in]  */,
							     const char *local_machine /* [in] [unique,charset(UTF16)] */,
							     uint32_t printer_local /* [in]  */,
							     struct spoolss_NotifyOption *notify_options /* [in] [unique] */,
							     WERROR *werror);
NTSTATUS rpccli_spoolss_RouterReplyPrinterEx(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     struct policy_handle *handle /* [in] [ref] */,
					     uint32_t color /* [in]  */,
					     uint32_t flags /* [in]  */,
					     uint32_t *reply_result /* [out] [ref] */,
					     uint32_t reply_type /* [in]  */,
					     union spoolss_ReplyPrinterInfo info /* [in] [switch_is(reply_type)] */,
					     WERROR *werror);
NTSTATUS rpccli_spoolss_RouterRefreshPrinterChangeNotify(struct rpc_pipe_client *cli,
							 TALLOC_CTX *mem_ctx,
							 struct policy_handle *handle /* [in] [ref] */,
							 uint32_t change_low /* [in]  */,
							 struct spoolss_NotifyOption *options /* [in] [unique] */,
							 struct spoolss_NotifyInfo **info /* [out] [ref] */,
							 WERROR *werror);
NTSTATUS rpccli_spoolss_44(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_OpenPrinterEx(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      const char *printername /* [in] [unique,charset(UTF16)] */,
				      const char *datatype /* [in] [unique,charset(UTF16)] */,
				      struct spoolss_DevmodeContainer devmode_ctr /* [in]  */,
				      uint32_t access_mask /* [in]  */,
				      uint32_t level /* [in]  */,
				      union spoolss_UserLevel userlevel /* [in] [switch_is(level)] */,
				      struct policy_handle *handle /* [out] [ref] */,
				      WERROR *werror);
NTSTATUS rpccli_spoolss_AddPrinterEx(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *server /* [in] [unique,charset(UTF16)] */,
				     struct spoolss_SetPrinterInfoCtr *info_ctr /* [in] [ref] */,
				     struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
				     struct sec_desc_buf *secdesc_ctr /* [in] [ref] */,
				     struct spoolss_UserLevelCtr *userlevel_ctr /* [in] [ref] */,
				     struct policy_handle *handle /* [out] [ref] */,
				     WERROR *werror);
NTSTATUS rpccli_spoolss_47(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_EnumPrinterData(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					uint32_t enum_index /* [in]  */,
					const char *value_name /* [out] [charset(UTF16),size_is(value_offered/2)] */,
					uint32_t value_offered /* [in]  */,
					uint32_t *value_needed /* [out] [ref] */,
					enum winreg_Type *type /* [out] [ref] */,
					uint8_t *data /* [out] [ref,flag(LIBNDR_PRINT_ARRAY_HEX),size_is(data_offered)] */,
					uint32_t data_offered /* [in]  */,
					uint32_t *data_needed /* [out] [ref] */,
					WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinterData(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in] [ref] */,
					  const char *value_name /* [in] [charset(UTF16)] */,
					  WERROR *werror);
NTSTATUS rpccli_spoolss_4a(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_4b(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_4c(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_SetPrinterDataEx(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 const char *key_name /* [in] [charset(UTF16)] */,
					 const char *value_name /* [in] [charset(UTF16)] */,
					 enum winreg_Type type /* [in]  */,
					 uint8_t *buffer /* [in] [ref,size_is(offered)] */,
					 uint32_t offered /* [in]  */,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_GetPrinterDataEx(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 const char *key_name /* [in] [charset(UTF16)] */,
					 const char *value_name /* [in] [charset(UTF16)] */,
					 enum winreg_Type *type /* [out] [ref] */,
					 uint8_t *buffer /* [out] [ref,size_is(offered)] */,
					 uint32_t offered /* [in]  */,
					 uint32_t *needed /* [out] [ref] */,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_EnumPrinterDataEx(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in] [ref] */,
					  const char *key_name /* [in] [charset(UTF16)] */,
					  uint32_t offered /* [in]  */,
					  uint32_t *count /* [out] [ref] */,
					  struct spoolss_PrinterEnumValues **info /* [out] [ref,size_is(,*count)] */,
					  uint32_t *needed /* [out] [ref] */,
					  WERROR *werror);
NTSTATUS rpccli_spoolss_EnumPrinterKey(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       const char *key_name /* [in] [charset(UTF16)] */,
				       uint32_t *_ndr_size /* [out] [ref] */,
				       union spoolss_KeyNames *key_buffer /* [out] [subcontext_size(*_ndr_size*2),ref,subcontext(0),switch_is(*_ndr_size)] */,
				       uint32_t offered /* [in]  */,
				       uint32_t *needed /* [out] [ref] */,
				       WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinterDataEx(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *handle /* [in] [ref] */,
					    const char *key_name /* [in] [charset(UTF16)] */,
					    const char *value_name /* [in] [charset(UTF16)] */,
					    WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinterKey(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 const char *key_name /* [in] [charset(UTF16)] */,
					 WERROR *werror);
NTSTATUS rpccli_spoolss_53(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_DeletePrinterDriverEx(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx,
					      const char *server /* [in] [unique,charset(UTF16)] */,
					      const char *architecture /* [in] [charset(UTF16)] */,
					      const char *driver /* [in] [charset(UTF16)] */,
					      uint32_t delete_flags /* [in]  */,
					      uint32_t version /* [in]  */,
					      WERROR *werror);
NTSTATUS rpccli_spoolss_55(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_56(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_57(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_XcvData(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				const char *function_name /* [in] [charset(UTF16)] */,
				DATA_BLOB in_data /* [in]  */,
				uint32_t _in_data_length /* [in] [value(r->in.in_data.length)] */,
				uint8_t *out_data /* [out] [ref,size_is(out_data_size)] */,
				uint32_t out_data_size /* [in]  */,
				uint32_t *needed /* [out] [ref] */,
				uint32_t *status_code /* [in,out] [ref] */,
				WERROR *werror);
NTSTATUS rpccli_spoolss_AddPrinterDriverEx(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   const char *servername /* [in] [unique,charset(UTF16)] */,
					   struct spoolss_AddDriverInfoCtr *info_ctr /* [in] [ref] */,
					   uint32_t flags /* [in]  */,
					   WERROR *werror);
NTSTATUS rpccli_spoolss_5a(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_5b(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_5c(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_5d(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_5e(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
NTSTATUS rpccli_spoolss_5f(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
#endif /* __CLI_SPOOLSS__ */
