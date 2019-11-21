#include "StdAfx.h"
#include "OptionsParser.h"
#include <errno.h>

int OptionsList::ParseLines( const wchar_t* Input )
{
	int nNumOptsFound = 0;
	
	const wchar_t *wszBufPtr = Input;
	while (wszBufPtr && *wszBufPtr)
	{
		size_t nEntryLen = wcslen(wszBufPtr);

		// Skip leading spaces and/or tabs
		while (*wszBufPtr == ' ' || *wszBufPtr == '\t')
			wszBufPtr++;

		// Skip commented entries
		if (wszBufPtr[0] != ';')
		{
			wchar_t *wszEntry = _wcsdup(wszBufPtr);
			wchar_t *wszSeparator = wcschr(wszEntry, '=');
			if (wszSeparator)
			{
				*wszSeparator = 0;
				AddOption(wszEntry, wszSeparator + 1);
				nNumOptsFound++;
			}
			free(wszEntry);
		}

		wszBufPtr += nEntryLen + 1;
	} //while

	return nNumOptsFound;
}

bool OptionsList::AddOption( const wchar_t* Key, const wchar_t* Value )
{
	// Ignore too large options
	if ((wcslen(Key) < OPT_KEY_MAXLEN) && (wcslen(Value) < OPT_VAL_MAXLEN))
	{
		OptionsItem opt;
		wcscpy_s(opt.key, OPT_KEY_MAXLEN, Key);
		wcscpy_s(opt.value, OPT_VAL_MAXLEN, Value);
		m_vValues.push_back(opt);

		return true;
	}
	
	return false;
}

bool OptionsList::GetValue( const wchar_t* Key, bool &Value ) const
{
	wchar_t tmpBuf[10] = {0};
	if ( GetValue(Key, tmpBuf, sizeof(tmpBuf) / sizeof(tmpBuf[0])) )
	{
		Value = (_wcsicmp(tmpBuf, L"true") == 0) || (wcscmp(tmpBuf, L"1") == 0);
		return true;
	}

	return false;
}

bool OptionsList::GetValue( const wchar_t* Key, int &Value ) const
{
	wchar_t tmpBuf[10] = {0};
	if ( GetValue(Key, tmpBuf, sizeof(tmpBuf) / sizeof(tmpBuf[0])) )
	{
		int resVal = wcstol(tmpBuf, NULL, 10);
		
		if (resVal != 0 || errno != EINVAL)
		{
			Value = resVal;
			return true;
		}
	}

	return false;
}

bool OptionsList::GetValue( const wchar_t* Key, wchar_t *Value, size_t MaxValueSize ) const
{
	for (auto cit = m_vValues.begin(); cit != m_vValues.end(); ++cit)
	{
		if (_wcsicmp(cit->key, Key) == 0)
		{
			if (wcslen(cit->value) >= MaxValueSize) break;

			wcscpy_s(Value, MaxValueSize, cit->value);
			return true;
		}
	} // for

	return false;
}
