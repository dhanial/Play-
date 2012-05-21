#include "CallStackWnd.h"
#include "PtrMacro.h"
#include "string_cast.h"
#include "lexical_cast_ex.h"
#include "../MIPS.h"

#define CLSNAME		_T("CallStackWnd")

CCallStackWnd::CCallStackWnd(HWND hParent, CVirtualMachine& virtualMachine, CMIPS* pCtx) :
m_virtualMachine(virtualMachine),
m_pCtx(pCtx)
{
	RECT rc;

	if(!DoesWindowClassExist(CLSNAME))
	{
		WNDCLASSEX wc;
		memset(&wc, 0, sizeof(WNDCLASSEX));
		wc.cbSize			= sizeof(WNDCLASSEX);
		wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground	= (HBRUSH)(COLOR_WINDOW); 
		wc.hInstance		= GetModuleHandle(NULL);
		wc.lpszClassName	= CLSNAME;
		wc.lpfnWndProc		= CWindow::WndProc;
		RegisterClassEx(&wc);
	}
	
	SetRect(&rc, 0, 0, 320, 240);

	Create(NULL, CLSNAME, _T("Call Stack"), WS_CLIPCHILDREN | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_CHILD | WS_MAXIMIZEBOX, &rc, hParent, NULL);
	SetClassPtr();

	SetRect(&rc, 0, 0, 1, 1);

	m_pList = new Framework::Win32::CListView(m_hWnd, &rc, LVS_REPORT);
	m_pList->SetExtendedListViewStyle(m_pList->GetExtendedListViewStyle() | LVS_EX_FULLROWSELECT);

	m_virtualMachine.OnMachineStateChange.connect(boost::bind(&CCallStackWnd::OnMachineStateChange, this));
	m_virtualMachine.OnRunningStateChange.connect(boost::bind(&CCallStackWnd::OnRunningStateChange, this));

	CreateColumns();

	RefreshLayout();
	Update();
}

CCallStackWnd::~CCallStackWnd()
{
	DELETEPTR(m_pList);
}

long CCallStackWnd::OnSize(unsigned int nType, unsigned int nX, unsigned int nY)
{
	RefreshLayout();
	return TRUE;
}

long CCallStackWnd::OnSysCommand(unsigned int nCmd, LPARAM lParam)
{
	switch(nCmd)
	{
	case SC_CLOSE:
		Show(SW_HIDE);
		return FALSE;
	}
	return TRUE;
}

long CCallStackWnd::OnNotify(WPARAM wParam, NMHDR* pHDR)
{
	switch(pHDR->code)
	{
	case NM_DBLCLK:
		OnListDblClick();
		return FALSE;
		break;
	}

	return FALSE;
}

void CCallStackWnd::RefreshLayout()
{
	RECT rc;

	GetClientRect(&rc);

	if(m_pList != NULL)
	{
		m_pList->SetSize(rc.right, rc.bottom);
	}

	m_pList->GetClientRect(&rc);

	m_pList->SetColumnWidth(0, rc.right / 2);
	m_pList->SetColumnWidth(1, rc.right / 2);
}

void CCallStackWnd::CreateColumns()
{
	LVCOLUMN col;
	RECT rc;

	m_pList->GetClientRect(&rc);

	memset(&col, 0, sizeof(LVCOLUMN));
	col.pszText = _T("Function");
	col.mask	= LVCF_TEXT;
	m_pList->InsertColumn(0, &col);

	memset(&col, 0, sizeof(LVCOLUMN));
	col.pszText = _T("Caller");
	col.mask	= LVCF_TEXT;
	m_pList->InsertColumn(1, &col);
}

void CCallStackWnd::Update()
{
	uint32 nPC = m_pCtx->m_State.nPC;
	uint32 nRA = m_pCtx->m_State.nGPR[CMIPS::RA].nV[0];
	uint32 nSP = m_pCtx->m_State.nGPR[CMIPS::SP].nV[0];

	m_pList->SetRedraw(false);

	m_pList->DeleteAllItems();

	auto callStackItems = CMIPSAnalysis::GetCallStack(m_pCtx, nPC, nSP, nRA);

	if(callStackItems.size() == 0)
	{
		//Cannot go further
		LVITEM item;
		memset(&item, 0, sizeof(LVITEM));
		item.pszText	= _T("Call stack unavailable at this state.");
		item.mask		= LVIF_TEXT | LVIF_PARAM;
		item.lParam		= MIPS_INVALID_PC;
		m_pList->InsertItem(&item);

		m_pList->SetRedraw(true);
		return;
	}

	for(auto itemIterator(std::begin(callStackItems));
		itemIterator != std::end(callStackItems); itemIterator++)
	{
		const auto& callStackItem(*itemIterator);

		//Add the current function
		LVITEM item;
		memset(&item, 0, sizeof(LVITEM));
		item.pszText	= _T("");
		item.iItem		= m_pList->GetItemCount();
		item.mask		= LVIF_TEXT | LVIF_PARAM;
		item.lParam		= callStackItem.function;
		unsigned int i = m_pList->InsertItem(&item);

		const char* sName = m_pCtx->m_Functions.Find(callStackItem.function);

		m_pList->SetItemText(i, 0, (
			_T("0x") + lexical_cast_hex<std::tstring>(callStackItem.function, 8) +
			(sName != NULL ? (_T(" (") + string_cast<std::tstring>(sName) + _T(")")) : _T(""))
			).c_str());

		m_pList->SetItemText(i, 1, (
			_T("0x") + lexical_cast_hex<std::tstring>(callStackItem.caller, 8)
			).c_str());
	}

	m_pList->SetRedraw(true);
}

void CCallStackWnd::OnListDblClick()
{
	int nSelection;
	uint32 nAddress;

	nSelection = m_pList->GetSelection();
	if(nSelection != -1)
	{
		nAddress = m_pList->GetItemData(nSelection);
		if(nAddress != MIPS_INVALID_PC)
		{
			m_OnFunctionDblClick(nAddress);
		}
	}
}

void CCallStackWnd::OnMachineStateChange()
{
	Update();
}

void CCallStackWnd::OnRunningStateChange()
{
	Update();
}
