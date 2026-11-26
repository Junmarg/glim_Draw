/**
 * @file ImageHistory.cpp
 * @brief 실행 취소/다시 실행 기록 구현
 */
#include "pch.h"
#include "ImageHistory.h"

void CImageHistory::Push(ImageSnapshot&& before)
{
    for (const ImageSnapshot& s : m_redo)
        m_bytes -= s.Bytes();
    m_redo.clear();

    m_bytes += before.Bytes();
    m_undo.push_back(std::move(before));
    Trim();
}

bool CImageHistory::Undo(ImageSnapshot& current)
{
    if (m_undo.empty())
        return false;
    m_bytes += current.Bytes();
    m_redo.push_back(std::move(current));
    current = std::move(m_undo.back());
    m_bytes -= current.Bytes();
    m_undo.pop_back();
    return true;
}

bool CImageHistory::Redo(ImageSnapshot& current)
{
    if (m_redo.empty())
        return false;
    m_bytes += current.Bytes();
    m_undo.push_back(std::move(current));
    current = std::move(m_redo.back());
    m_bytes -= current.Bytes();
    m_redo.pop_back();
    Trim();
    return true;
}

void CImageHistory::Clear()
{
    m_undo.clear();
    m_redo.clear();
    m_bytes = 0;
}

void CImageHistory::Trim()
{
    while (m_undo.size() > 1 && (m_undo.size() > m_maxDepth || m_bytes > m_maxBytes))
    {
        m_bytes -= m_undo.front().Bytes();
        m_undo.pop_front();
    }
}
