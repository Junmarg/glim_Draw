#pragma once

/**
 * @file ImageHistory.h
 * @brief 실행 취소/다시 실행을 위한 이미지 스냅숏 기록
 */

#include <cstdint>
#include <deque>
#include <vector>

/**
 * @brief 이미지 한 장의 픽셀 사본 (32비트 BGRX, 위에서 아래 순서)
 */
struct ImageSnapshot
{
    int width = 0;                 ///< 너비 (픽셀)
    int height = 0;                ///< 높이 (픽셀)
    std::vector<uint32_t> pixels;  ///< width * height 개의 픽셀

    /** @brief 내용이 없는지 여부 */
    bool IsEmpty() const { return pixels.empty(); }

    /** @brief 차지하는 메모리 (바이트) */
    size_t Bytes() const { return pixels.size() * sizeof(uint32_t); }
};

/**
 * @brief 스냅숏 방식의 실행 취소/다시 실행 스택
 *
 * 작업을 확정할 때 "작업 전" 이미지를 Push 하고,
 * Undo/Redo 할 때는 현재 이미지를 넘겨받아 반대쪽 스택으로 옮깁니다.
 */
class CImageHistory
{
public:
    /**
     * @brief 생성자
     * @param maxDepth 실행 취소 스택에 보관할 최대 단계 수
     * @param maxBytes 실행 취소 + 다시 실행 기록 전체가 쓸 수 있는 최대 메모리.
     *                 넘으면 오래된 단계부터 버립니다. (최소 1단계는 유지)
     */
    explicit CImageHistory(size_t maxDepth = 50, size_t maxBytes = size_t(512) * 1024 * 1024)
        : m_maxDepth(maxDepth), m_maxBytes(maxBytes) {}

    /**
     * @brief 작업 전 이미지를 기록합니다. 다시 실행 스택은 비워집니다.
     * @param before 작업을 시작하기 전의 이미지
     */
    void Push(ImageSnapshot&& before);

    /** @brief 실행 취소할 수 있는지 여부 */
    bool CanUndo() const { return !m_undo.empty(); }

    /** @brief 다시 실행할 수 있는지 여부 */
    bool CanRedo() const { return !m_redo.empty(); }

    /**
     * @brief 실행 취소. current를 다시 실행 스택에 넣고, 직전 이미지로 바꿉니다.
     * @param current [in] 현재 이미지, [out] 되돌린 이미지
     * @return 실행 취소했으면 true
     */
    bool Undo(ImageSnapshot& current);

    /**
     * @brief 다시 실행. current를 실행 취소 스택에 넣고, 다음 이미지로 바꿉니다.
     * @param current [in] 현재 이미지, [out] 다시 실행한 이미지
     * @return 다시 실행했으면 true
     */
    bool Redo(ImageSnapshot& current);

    /** @brief 모든 기록을 지웁니다. */
    void Clear();

private:
    /** @brief 단계 수와 메모리 한도를 넘으면 가장 오래된 실행 취소 단계부터 버립니다. */
    void Trim();

    size_t m_maxDepth;                  ///< 최대 단계 수
    size_t m_maxBytes;                  ///< 최대 메모리 (바이트)
    size_t m_bytes = 0;                 ///< 현재 두 스택이 쓰는 메모리 (바이트)
    std::deque<ImageSnapshot> m_undo;   ///< 실행 취소 스택 (뒤가 최신)
    std::deque<ImageSnapshot> m_redo;   ///< 다시 실행 스택 (뒤가 최신)
};
