#include "..\..\Settings.h"
#include "..\..\User Interface.h"
#include "SettingsType-TempString.h"

CSettingTypeTempString::CSettingTypeTempString(LPCSTR initialValue) :
	m_value(initialValue)
{
}

bool CSettingTypeTempString::Load ( int Index, bool & Value ) const
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

bool CSettingTypeTempString::Load ( int Index, ULONG & Value ) const
{
	Notify().BreakPoint(__FILE__,__LINE__); 
	return false;
}

bool CSettingTypeTempString::Load ( int Index, stdstr & Value ) const
{
	Value = m_value;
	return true;
}

//return the default values
void CSettingTypeTempString::LoadDefault ( int Index, bool & Value   ) const
{
	Notify().BreakPoint(__FILE__,__LINE__);
}

void CSettingTypeTempString::LoadDefault ( int Index, ULONG & Value  ) const
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}

void CSettingTypeTempString::LoadDefault ( int Index, stdstr & Value ) const
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}

void CSettingTypeTempString::Save ( int Index, bool Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}

void CSettingTypeTempString::Save ( int Index, ULONG Value )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}

void CSettingTypeTempString::Save ( int Index, const stdstr & Value )
{
	m_value = Value;
}

void CSettingTypeTempString::Save ( int Index, const char * Value )
{
	m_value = Value;
}

void CSettingTypeTempString::Delete( int Index )
{
	Notify().BreakPoint(__FILE__,__LINE__); 
}