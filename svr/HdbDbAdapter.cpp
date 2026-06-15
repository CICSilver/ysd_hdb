#include "HdbDbAdapter.h"

#include <string.h>

void CHdbQueryResult::Clear()
{
    m_columns.clear();
    m_rows.clear();
}

int CHdbQueryResult::RowCount() const
{
    return (int)m_rows.size();
}

int CHdbQueryResult::FieldCount() const
{
    return (int)m_columns.size();
}

const char* CHdbQueryResult::GetColumnName(int field) const
{
    if (field < 0 || field >= (int)m_columns.size())
    {
        return "";
    }
    return m_columns[field].c_str();
}

int CHdbQueryResult::FindColumn(const char* name) const
{
    int i;

    if (name == NULL)
    {
        return -1;
    }
    for (i = 0; i < (int)m_columns.size(); ++i)
    {
        if (strcmp(m_columns[i].c_str(), name) == 0)
        {
            return i;
        }
    }
    return -1;
}

const char* CHdbQueryResult::GetValue(int row, int field) const
{
    if (row < 0 || row >= (int)m_rows.size())
    {
        return "";
    }
    if (field < 0 || field >= (int)m_rows[row].size())
    {
        return "";
    }
    return m_rows[row][field].value.c_str();
}

int CHdbQueryResult::IsNull(int row, int field) const
{
    if (row < 0 || row >= (int)m_rows.size())
    {
        return 1;
    }
    if (field < 0 || field >= (int)m_rows[row].size())
    {
        return 1;
    }
    return m_rows[row][field].isNull != 0 ? 1 : 0;
}

void CHdbQueryResult::AddColumn(const std::string& name)
{
    m_columns.push_back(name);
}

void CHdbQueryResult::SetColumnName(int field, const std::string& name)
{
    if (field < 0 || field >= (int)m_columns.size())
    {
        return;
    }
    m_columns[field] = name;
}

void CHdbQueryResult::SetValue(int row, int field, const std::string& value, int isNull)
{
    if (row < 0 || row >= (int)m_rows.size())
    {
        return;
    }
    if (field < 0 || field >= (int)m_rows[row].size())
    {
        return;
    }
    m_rows[row][field].value = value;
    m_rows[row][field].isNull = isNull != 0 ? 1 : 0;
}

void CHdbQueryResult::AddRow(const std::vector<HdbQueryCell>& row)
{
    m_rows.push_back(row);
}
