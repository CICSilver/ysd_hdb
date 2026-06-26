/**
 * @file hdb_ini.h
 * @brief hdb_ini 配置读写单头文件
 *
 * 本文件用于读取和写出项目专用 INI-like 配置，主要服务于代码生成阶段中数据库
 * 无法直接推导出的附加元数据配置
 * 支持重复 section、key=value 字段、文件读写、规范化输出和带行号的错误定位
 *
 * =========================
 * 1. 基本定义
 * =========================
 * - CHdbIniDocument : 配置文档对象，负责解析、保存和遍历 section
 * - HdbIniSection   : 配置段对象，一个 section 表示一条配置记录
 * - HdbIniItem      : 配置项对象，保存 key、value 和来源行号
 * - HdbIniErrorCode : 解析和读写错误码
 *
 * =========================
 * 2. 配置格式
 * =========================
 * - 空行会被忽略
 * - 行首第一个非空白字符是 # 或 ; 时整行作为注释
 * - section 写成 [association]
 * - key=value 按第一个等号切分
 * - key 和 value 两侧空白会被裁剪
 * - value 可以为空
 *
 * 2.1 association 示例
 *   [association]
 *   owner=record
 *   name=creator
 *   target=user
 *   owner_field=creator_id
 *   target_field=id
 *
 * =========================
 * 3. 常用接口与用法
 * =========================
 * 3.1 从内存文本读取
 *   CHdbIniDocument doc;
 *   int ret = doc.Parse(text);
 *   if (ret != HDB_INI_OK)
 *   {
 *       int line = doc.GetLastErrorLine();
 *       const char* error = doc.GetLastError();
 *   }
 *
 * 3.2 遍历同名 section
 *   int index = doc.FindFirstSection("association");
 *   while (index >= 0)
 *   {
 *       const HdbIniSection* section = doc.GetSection(index);
 *       const char* owner = section->GetValue("owner");
 *       index = doc.FindNextSection("association", index + 1);
 *   }
 *
 * 3.3 写出配置
 *   doc.Serialize(text);
 *   doc.Save("hdb_codegen.ini");
 *
 * =========================
 * 4. 注意事项
 * =========================
 * - 不支持 inline comment
 * - 不支持引号、转义和多行 value
 * - 不支持嵌套 section
 * - section 名和 key 名只允许字母、数字、下划线，且必须以字母或下划线开头
 * - 同一 section 内重复 key 会报错
 * - Serialize 和 Save 只写出规范化后的 section 和 key=value
 * - 原始注释和空行不会保留
 * - 输出换行统一为 CRLF
 *
 * =========================
 * 5. 主要接口
 * =========================
 * - CHdbIniDocument::Parse(text)                 解析内存文本
 * - CHdbIniDocument::Load(path)                  从文件读取
 * - CHdbIniDocument::Serialize(outText)          输出规范化文本
 * - CHdbIniDocument::Save(path)                  写出规范化文件
 * - CHdbIniDocument::AddSection(name, outIndex)  添加 section
 * - CHdbIniDocument::AddValue(index, key, value) 添加 key=value
 * - CHdbIniDocument::FindFirstSection(name)      查找第一个同名 section
 * - CHdbIniDocument::FindNextSection(name, idx)  从指定位置继续查找 section
 * - HdbIniSection::GetValue(key)                 读取 section 内指定 value
 */
#pragma once
#ifndef YSD_HDB_INI_H
#define YSD_HDB_INI_H

#include <ctype.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

enum HdbIniErrorCode
{
    HDB_INI_OK = 0,
    HDB_INI_ERR_PARAM = -1,
    HDB_INI_ERR_IO = -2,
    HDB_INI_ERR_SECTION = -3,
    HDB_INI_ERR_KEY_VALUE = -4,
    HDB_INI_ERR_DUPLICATE_KEY = -5,
    HDB_INI_ERR_NAME = -6
};

struct HdbIniItem
{
    std::string key;
    std::string value;
    int line;

    HdbIniItem()
        : key(),
          value(),
          line(0)
    {
    }
};

struct HdbIniSection
{
    std::string name;
    int line;
    std::vector<HdbIniItem> items;

    HdbIniSection()
        : name(),
          line(0),
          items()
    {
    }

    int FindItem(const char* keyText) const
    {
        int i;

        if (keyText == NULL)
        {
            return -1;
        }
        for (i = 0; i < (int)items.size(); ++i)
        {
            if (items[i].key == keyText)
            {
                return i;
            }
        }
        return -1;
    }

    const char* GetValue(const char* keyText) const
    {
        int index;

        index = FindItem(keyText);
        if (index < 0)
        {
            return NULL;
        }
        return items[index].value.c_str();
    }
};

class CHdbIniDocument
{
public:
    CHdbIniDocument()
        : m_sections(),
          m_lastError(),
          m_lastErrorLine(0)
    {
    }

    void Clear()
    {
        m_sections.clear();
        ClearError();
    }

    int Parse(const char* text)
    {
        if (text == NULL)
        {
            m_sections.clear();
            return Fail(0, "input text is NULL", HDB_INI_ERR_PARAM);
        }
        return Parse(std::string(text));
    }

    int Parse(const std::string& text)
    {
        std::vector<HdbIniSection> parsedSections;
        size_t pos;
        int lineNo;
        int currentSection;

        ClearError();
        pos = 0;
        lineNo = 1;
        currentSection = -1;
        while (pos <= text.size())
        {
            size_t end;
            std::string line;
            int ret;

            end = pos;
            while (end < text.size() && text[end] != '\r' && text[end] != '\n')
            {
                ++end;
            }
            line = text.substr(pos, end - pos);
            ret = ParseLine(line, lineNo, parsedSections, currentSection);
            if (ret != HDB_INI_OK)
            {
                m_sections.clear();
                return ret;
            }
            if (end >= text.size())
            {
                break;
            }
            if (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n')
            {
                pos = end + 2;
            }
            else
            {
                pos = end + 1;
            }
            ++lineNo;
        }
        m_sections = parsedSections;
        return HDB_INI_OK;
    }

    int Load(const char* path)
    {
        std::ifstream input;
        std::ostringstream buffer;

        if (path == NULL || path[0] == '\0')
        {
            m_sections.clear();
            return Fail(0, "input path is empty", HDB_INI_ERR_PARAM);
        }
        input.open(path, std::ios::in | std::ios::binary);
        if (!input)
        {
            m_sections.clear();
            return Fail(0, "open input file failed", HDB_INI_ERR_IO);
        }
        buffer << input.rdbuf();
        if (!input.good() && !input.eof())
        {
            m_sections.clear();
            return Fail(0, "read input file failed", HDB_INI_ERR_IO);
        }
        return Parse(buffer.str());
    }

    int Serialize(std::string& outText) const
    {
        int i;

        outText.clear();
        for (i = 0; i < (int)m_sections.size(); ++i)
        {
            int j;

            if (i > 0)
            {
                outText += "\r\n";
            }
            outText += "[";
            outText += m_sections[i].name;
            outText += "]\r\n";
            for (j = 0; j < (int)m_sections[i].items.size(); ++j)
            {
                outText += m_sections[i].items[j].key;
                outText += "=";
                outText += m_sections[i].items[j].value;
                outText += "\r\n";
            }
        }
        return HDB_INI_OK;
    }

    int Save(const char* path)
    {
        std::ofstream output;
        std::string text;

        if (path == NULL || path[0] == '\0')
        {
            return Fail(0, "output path is empty", HDB_INI_ERR_PARAM);
        }
        Serialize(text);
        output.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return Fail(0, "open output file failed", HDB_INI_ERR_IO);
        }
        output.write(text.c_str(), (std::streamsize)text.size());
        if (!output)
        {
            return Fail(0, "write output file failed", HDB_INI_ERR_IO);
        }
        ClearError();
        return HDB_INI_OK;
    }

    int AddSection(const char* name, int* outIndex)
    {
        HdbIniSection section;

        if (outIndex != NULL)
        {
            *outIndex = -1;
        }
        if (!IsNameText(name))
        {
            return Fail(0, "invalid section name", HDB_INI_ERR_NAME);
        }
        section.name = name;
        section.line = 0;
        m_sections.push_back(section);
        if (outIndex != NULL)
        {
            *outIndex = (int)m_sections.size() - 1;
        }
        ClearError();
        return HDB_INI_OK;
    }

    int AddValue(int sectionIndex, const char* key, const char* value)
    {
        HdbIniItem item;

        if (sectionIndex < 0 || sectionIndex >= (int)m_sections.size())
        {
            return Fail(0, "section index is out of range", HDB_INI_ERR_PARAM);
        }
        if (!IsNameText(key))
        {
            return Fail(0, "invalid key name", HDB_INI_ERR_NAME);
        }
        if (m_sections[sectionIndex].FindItem(key) >= 0)
        {
            return Fail(0, "duplicate key in section", HDB_INI_ERR_DUPLICATE_KEY);
        }
        item.key = key;
        item.value = value != NULL ? value : "";
        item.line = 0;
        m_sections[sectionIndex].items.push_back(item);
        ClearError();
        return HDB_INI_OK;
    }

    int GetSectionCount() const
    {
        return (int)m_sections.size();
    }

    const HdbIniSection* GetSection(int index) const
    {
        if (index < 0 || index >= (int)m_sections.size())
        {
            return NULL;
        }
        return &m_sections[index];
    }

    int FindFirstSection(const char* name) const
    {
        return FindNextSection(name, 0);
    }

    int FindNextSection(const char* name, int beginIndex) const
    {
        int i;

        if (name == NULL || beginIndex < 0)
        {
            return -1;
        }
        for (i = beginIndex; i < (int)m_sections.size(); ++i)
        {
            if (m_sections[i].name == name)
            {
                return i;
            }
        }
        return -1;
    }

    const char* GetLastError() const
    {
        return m_lastError.c_str();
    }

    int GetLastErrorLine() const
    {
        return m_lastErrorLine;
    }

private:
    int ParseLine(const std::string& rawLine,
        int lineNo,
        std::vector<HdbIniSection>& sections,
        int& currentSection)
    {
        std::string line;

        line = rawLine;
        if (lineNo == 1)
        {
            StripUtf8Bom(line);
        }
        line = TrimCopy(line);
        if (line.empty())
        {
            return HDB_INI_OK;
        }
        if (line[0] == '#' || line[0] == ';')
        {
            return HDB_INI_OK;
        }
        if (line[0] == '[')
        {
            return ParseSectionLine(line, lineNo, sections, currentSection);
        }
        return ParseKeyValueLine(line, lineNo, sections, currentSection);
    }

    int ParseSectionLine(const std::string& line,
        int lineNo,
        std::vector<HdbIniSection>& sections,
        int& currentSection)
    {
        HdbIniSection section;
        std::string name;

        if (line.size() < 3 || line[line.size() - 1] != ']')
        {
            return Fail(lineNo, "invalid section line", HDB_INI_ERR_SECTION);
        }
        name = TrimCopy(line.substr(1, line.size() - 2));
        if (!IsNameText(name.c_str()))
        {
            return Fail(lineNo, "invalid section name", HDB_INI_ERR_NAME);
        }
        section.name = name;
        section.line = lineNo;
        sections.push_back(section);
        currentSection = (int)sections.size() - 1;
        return HDB_INI_OK;
    }

    int ParseKeyValueLine(const std::string& line,
        int lineNo,
        std::vector<HdbIniSection>& sections,
        int currentSection)
    {
        HdbIniItem item;
        size_t eqPos;

        if (currentSection < 0 || currentSection >= (int)sections.size())
        {
            return Fail(lineNo, "key value line has no section", HDB_INI_ERR_KEY_VALUE);
        }
        eqPos = line.find('=');
        if (eqPos == std::string::npos)
        {
            return Fail(lineNo, "missing equal sign", HDB_INI_ERR_KEY_VALUE);
        }
        item.key = TrimCopy(line.substr(0, eqPos));
        item.value = TrimCopy(line.substr(eqPos + 1));
        item.line = lineNo;
        if (!IsNameText(item.key.c_str()))
        {
            return Fail(lineNo, "invalid key name", HDB_INI_ERR_NAME);
        }
        if (sections[currentSection].FindItem(item.key.c_str()) >= 0)
        {
            return Fail(lineNo, "duplicate key in section", HDB_INI_ERR_DUPLICATE_KEY);
        }
        sections[currentSection].items.push_back(item);
        return HDB_INI_OK;
    }

    static void StripUtf8Bom(std::string& line)
    {
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xef &&
            (unsigned char)line[1] == 0xbb &&
            (unsigned char)line[2] == 0xbf)
        {
            line.erase(0, 3);
        }
    }

    static std::string TrimCopy(const std::string& text)
    {
        size_t begin;
        size_t end;

        begin = 0;
        end = text.size();
        while (begin < end && IsTrimChar(text[begin]))
        {
            ++begin;
        }
        while (end > begin && IsTrimChar(text[end - 1]))
        {
            --end;
        }
        return text.substr(begin, end - begin);
    }

    static int IsTrimChar(char ch)
    {
        return ch == ' ' || ch == '\t';
    }

    static int IsNameText(const char* text)
    {
        int i;

        if (text == NULL || text[0] == '\0')
        {
            return 0;
        }
        if (!(isalpha((unsigned char)text[0]) || text[0] == '_'))
        {
            return 0;
        }
        for (i = 1; text[i] != '\0'; ++i)
        {
            if (!(isalnum((unsigned char)text[i]) || text[i] == '_'))
            {
                return 0;
            }
        }
        return 1;
    }

    void ClearError()
    {
        m_lastError.clear();
        m_lastErrorLine = 0;
    }

    int Fail(int line, const char* text, int code)
    {
        m_lastError = text != NULL ? text : "";
        m_lastErrorLine = line;
        return code;
    }

private:
    std::vector<HdbIniSection> m_sections;
    std::string m_lastError;
    int m_lastErrorLine;
};

#endif
