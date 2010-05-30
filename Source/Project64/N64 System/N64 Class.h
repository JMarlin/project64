#pragma once

typedef std::list<SystemEvent>   EVENT_LIST;

typedef struct {
	stdstr       FileName;
	void       * ThreadHandle;
	DWORD        ThreadID;
} FileImageInfo;

typedef std::map<DWORD, DWORD> FUNC_CALLS;

class CPlugins;
class CRSP_Plugin;
class CC_Core;

//#define TEST_SP_TRACKING  //track the SP to make sure all ops pick it up fine

class CN64System :
	private CMipsMemory_CallBack,
	private CTLB_CB,
	protected CN64SystemSettings,
	public CDebugger
{
public:
         CN64System ( CPlugins * Plugins, bool SavesReadOnly );
        ~CN64System ( void );

	//Methods
	static bool CN64System::RunFileImage ( const char * FileLoc );
		
	void   CloseCpu         ( void );
	void   ExternalEvent    ( SystemEvent Event ); //covers gui interacting and timers etc..
	stdstr ChooseFileToOpen ( WND_HANDLE hParent );
	void   DisplayRomInfo   ( WND_HANDLE hParent );
	void   SelectCheats     ( WND_HANDLE hParent );
	void   StartEmulation   ( bool NewThread );
	void   SyncToAudio      ( void );
	bool   IsDialogMsg      ( MSG * msg );
	void   IncreaseSpeed    ( void ) { m_Limitor.IncreaeSpeed(10); }
	void   DecreaeSpeed     ( void ) { m_Limitor.DecreaeSpeed(10); }
	void   SoftReset        ( void );

	bool  m_EndEmulation;

	//Get the pointer to the Internal Classes
	CRecompiler * GetRecompiler ( void ) { return m_Recomp; }
	//CN64Rom     * GetCurrentRom ( void ) { return _Rom; }

	inline CPlugins * Plugins ( void ) const { return m_Plugins; }

	//Variable used to track that the SP is being handled and stays the same as the real SP in sync core
#ifdef TEST_SP_TRACKING
	DWORD m_CurrentSP;
#endif
	//For Sync CPU
	void   UpdateSyncCPU    ( CN64System * const SecondCPU, DWORD const Cycles );
	void   SyncCPU          ( CN64System * const SecondCPU );

private:
	//Make sure plugins can directly access this information
	friend CGfxPlugin;
	friend CAudioPlugin;
	friend CRSP_Plugin;
	friend CControl_Plugin;
	
	//Recompiler has access to manipulate and call functions
	friend CC_Core;

	//Used for loading and potentialy executing the CPU in its own thread.
	static void stLoadFileImage      ( FileImageInfo * Info );
	static void StartEmulationThread ( FileImageInfo * Info );

	void   ExecuteCycles    ( DWORD Cycles );
	void   ExecuteCPU       ( void );
	void   RefreshScreen    ( void );
	bool   InternalEvent    ( void );
	bool   InPermLoop       ( void );
	void   Reset            ( void );
	void   RunRSP           ( void );
	bool   SaveState        ( void );
	bool   LoadState        ( LPCSTR FileName );
	bool   LoadState        ( void );
	void   DumpSyncErrors   ( CN64System * SecondCPU );
	void   StartEmulation2  ( bool NewThread );
	bool   SetActiveSystem  ( bool bActive );
	void   InitRegisters    ( bool bPostPif, CMipsMemory & MMU );

	//CPU Methods
	void   ExecuteRecompiler ( CC_Core & C_Core );
	void   ExecuteInterpret  ( CC_Core & C_Core );
	void   ExecuteSyncCPU    ( CC_Core & C_Core );

	void   AddEvent          ( SystemEvent Event);

	//Notification of changing conditions
	void   FunctionStarted ( DWORD NewFuncAddress, DWORD OldFuncAddress, DWORD ReturnAddress );
	void   FunctionEnded   ( DWORD ReturnAddress, DWORD StackPos );

	//Mark information saying that the CPU has stoped
	void   CpuStopped      ( void );
	void   Pause           ( void );

	static void PluginChanged ( CN64System * _this );

	//Function in CMipsMemory_CallBack
	virtual bool WriteToProtectedMemory (DWORD Address, int length);

	//Functions in CTLB_CB
	void TLB_Mapped  ( DWORD VAddr, DWORD Len, DWORD PAddr, bool bReadOnly );
	void TLB_Unmaped ( DWORD VAddr, DWORD Len );
	void TLB_Changed ( void );

    CPlugins      * const m_Plugins;  //The plugin container 
	CN64System    * m_SyncCPU;
	CMipsMemoryVM  m_MMU_VM;   //Memory of the n64 
	CTLB           m_TLB;
	CRegisters     m_Reg;   
	CCheats         m_Cheats;
	CFramePerSecond m_FPS;
	CProfiling      m_CPU_Usage, m_Profile; //used to track the cpu usage
	CRecompiler     * m_Recomp;
	CAudio          m_Audio;
	CSpeedLimitor   m_Limitor;
	bool            m_InReset;
	CSystemTimer    m_SystemTimer;
	SystemType      m_SystemType;
	bool            m_bCleanFrameBox;
	bool            m_bInitilized;
	int             m_NextTimer;
	
	//When Syncing cores this is the PC where it last Sync'ed correctly
	DWORD m_LastSuccessSyncPC[10];
	int   m_CyclesToSkip;

	//List of Internal events that need to be acted on by CPU
	EVENT_LIST    m_EventList;
	DWORD         m_NoOfEvents;

	//Handle to the cpu thread
	HANDLE m_CPU_Handle;
	DWORD  m_CPU_ThreadID;
	
	//Handle to pause mutex
	void * m_hPauseEvent;

	//No of Alist and Dlist sent to the RSP
	DWORD m_AlistCount, m_DlistCount, m_UnknownCount;

	//list of function that have been called .. used in profiling
	FUNC_CALLS m_FunctionCalls;
};