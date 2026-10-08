#pragma once

// MFC collection classes (afxtempl.h / afxcoll.h).

#include <map>
#include <utility>
#include <vector>

struct __POSITION {};
typedef __POSITION* POSITION;
#define BEFORE_START_POSITION ((POSITION)(intptr_t)-1)

template <class TYPE>
inline void CopyElements(TYPE* pDest, const TYPE* pSrc, INT_PTR nCount) {
    for (INT_PTR i = 0; i < nCount; ++i)
        pDest[i] = pSrc[i];
}

template <class TYPE, class ARG_TYPE = const TYPE&>
class CArray : public CObject {
public:
    CArray() = default;
    CArray(const CArray& other) : CObject(), m_data(other.m_data) {}
    CArray& operator=(const CArray& other) { m_data = other.m_data; return *this; }

    INT_PTR GetSize() const { return static_cast<INT_PTR>(m_data.size()); }
    INT_PTR GetCount() const { return GetSize(); }
    BOOL IsEmpty() const { return m_data.empty(); }
    INT_PTR GetUpperBound() const { return GetSize() - 1; }
    void SetSize(INT_PTR nNewSize, INT_PTR nGrowBy = -1) {
        if (nGrowBy > 0)
            m_data.reserve(static_cast<size_t>(nNewSize + nGrowBy));
        m_data.resize(static_cast<size_t>(nNewSize < 0 ? 0 : nNewSize));
    }
    void FreeExtra() { m_data.shrink_to_fit(); }
    void RemoveAll() { m_data.clear(); }
    const TYPE& GetAt(INT_PTR nIndex) const { return m_data[static_cast<size_t>(nIndex)]; }
    TYPE& GetAt(INT_PTR nIndex) { return m_data[static_cast<size_t>(nIndex)]; }
    void SetAt(INT_PTR nIndex, ARG_TYPE newElement) { m_data[static_cast<size_t>(nIndex)] = newElement; }
    const TYPE& ElementAt(INT_PTR nIndex) const { return m_data[static_cast<size_t>(nIndex)]; }
    TYPE& ElementAt(INT_PTR nIndex) { return m_data[static_cast<size_t>(nIndex)]; }
    const TYPE* GetData() const { return m_data.empty() ? nullptr : m_data.data(); }
    TYPE* GetData() { return m_data.empty() ? nullptr : m_data.data(); }
    void SetAtGrow(INT_PTR nIndex, ARG_TYPE newElement) {
        if (nIndex >= GetSize())
            m_data.resize(static_cast<size_t>(nIndex + 1));
        m_data[static_cast<size_t>(nIndex)] = newElement;
    }
    INT_PTR Add(ARG_TYPE newElement) {
        m_data.push_back(newElement);
        return GetSize() - 1;
    }
    INT_PTR Append(const CArray& src) {
        INT_PTR old = GetSize();
        m_data.insert(m_data.end(), src.m_data.begin(), src.m_data.end());
        return old;
    }
    void Copy(const CArray& src) { m_data = src.m_data; }
    const TYPE& operator[](INT_PTR nIndex) const { return m_data[static_cast<size_t>(nIndex)]; }
    TYPE& operator[](INT_PTR nIndex) { return m_data[static_cast<size_t>(nIndex)]; }
    void InsertAt(INT_PTR nIndex, ARG_TYPE newElement, INT_PTR nCount = 1) {
        if (nIndex > GetSize())
            m_data.resize(static_cast<size_t>(nIndex));
        m_data.insert(m_data.begin() + nIndex, static_cast<size_t>(nCount), newElement);
    }
    void InsertAt(INT_PTR nStartIndex, CArray* pNewArray) {
        if (nStartIndex > GetSize())
            m_data.resize(static_cast<size_t>(nStartIndex));
        m_data.insert(m_data.begin() + nStartIndex, pNewArray->m_data.begin(), pNewArray->m_data.end());
    }
    void RemoveAt(INT_PTR nIndex, INT_PTR nCount = 1) {
        m_data.erase(m_data.begin() + nIndex, m_data.begin() + nIndex + nCount);
    }
    void Serialize(CArchive& ar) override;

protected:
    std::vector<TYPE> m_data;
};

template <class BASE_CLASS, class TYPE>
class CTypedPtrArray : public BASE_CLASS {
public:
    TYPE GetAt(INT_PTR nIndex) const { return static_cast<TYPE>(BASE_CLASS::GetAt(nIndex)); }
    TYPE& ElementAt(INT_PTR nIndex) { return reinterpret_cast<TYPE&>(BASE_CLASS::ElementAt(nIndex)); }
    void SetAt(INT_PTR nIndex, TYPE ptr) { BASE_CLASS::SetAt(nIndex, ptr); }
    void SetAtGrow(INT_PTR nIndex, TYPE newElement) { BASE_CLASS::SetAtGrow(nIndex, newElement); }
    INT_PTR Add(TYPE newElement) { return BASE_CLASS::Add(newElement); }
    void InsertAt(INT_PTR nIndex, TYPE newElement, INT_PTR nCount = 1) { BASE_CLASS::InsertAt(nIndex, newElement, nCount); }
    TYPE operator[](INT_PTR nIndex) const { return GetAt(nIndex); }
    TYPE& operator[](INT_PTR nIndex) { return ElementAt(nIndex); }
};

template <class TYPE, class ARG_TYPE = const TYPE&>
class CList : public CObject {
    struct CNode {
        CNode* pNext;
        CNode* pPrev;
        TYPE data;
    };

public:
    explicit CList(INT_PTR = 10) {}
    CList(const CList&) = delete;
    CList& operator=(const CList&) = delete;
    ~CList() override { RemoveAll(); }

    INT_PTR GetCount() const { return m_nCount; }
    INT_PTR GetSize() const { return m_nCount; }
    BOOL IsEmpty() const { return m_nCount == 0; }
    TYPE& GetHead() { return m_pHead->data; }
    const TYPE& GetHead() const { return m_pHead->data; }
    TYPE& GetTail() { return m_pTail->data; }
    const TYPE& GetTail() const { return m_pTail->data; }
    TYPE RemoveHead() {
        CNode* node = m_pHead;
        TYPE value = node->data;
        Unlink(node);
        return value;
    }
    TYPE RemoveTail() {
        CNode* node = m_pTail;
        TYPE value = node->data;
        Unlink(node);
        return value;
    }
    POSITION AddHead(ARG_TYPE newElement) { return InsertBefore(reinterpret_cast<POSITION>(m_pHead), newElement); }
    POSITION AddTail(ARG_TYPE newElement) { return InsertAfter(reinterpret_cast<POSITION>(m_pTail), newElement); }
    void AddHead(CList* pNewList) {
        for (CNode* n = pNewList->m_pTail; n; n = n->pPrev)
            AddHead(n->data);
    }
    void AddTail(CList* pNewList) {
        for (CNode* n = pNewList->m_pHead; n; n = n->pNext)
            AddTail(n->data);
    }
    void RemoveAll() {
        while (m_pHead)
            Unlink(m_pHead);
    }
    POSITION GetHeadPosition() const { return reinterpret_cast<POSITION>(m_pHead); }
    POSITION GetTailPosition() const { return reinterpret_cast<POSITION>(m_pTail); }
    TYPE& GetNext(POSITION& rPosition) {
        CNode* node = reinterpret_cast<CNode*>(rPosition);
        rPosition = reinterpret_cast<POSITION>(node->pNext);
        return node->data;
    }
    const TYPE& GetNext(POSITION& rPosition) const {
        CNode* node = reinterpret_cast<CNode*>(rPosition);
        rPosition = reinterpret_cast<POSITION>(node->pNext);
        return node->data;
    }
    TYPE& GetPrev(POSITION& rPosition) {
        CNode* node = reinterpret_cast<CNode*>(rPosition);
        rPosition = reinterpret_cast<POSITION>(node->pPrev);
        return node->data;
    }
    const TYPE& GetPrev(POSITION& rPosition) const {
        CNode* node = reinterpret_cast<CNode*>(rPosition);
        rPosition = reinterpret_cast<POSITION>(node->pPrev);
        return node->data;
    }
    TYPE& GetAt(POSITION position) { return reinterpret_cast<CNode*>(position)->data; }
    const TYPE& GetAt(POSITION position) const { return reinterpret_cast<CNode*>(position)->data; }
    void SetAt(POSITION pos, ARG_TYPE newElement) { reinterpret_cast<CNode*>(pos)->data = newElement; }
    void RemoveAt(POSITION position) { Unlink(reinterpret_cast<CNode*>(position)); }
    POSITION InsertBefore(POSITION position, ARG_TYPE newElement) {
        CNode* at = reinterpret_cast<CNode*>(position);
        CNode* node = new CNode{at, at ? at->pPrev : m_pTail, newElement};
        if (!at) {
            node->pNext = m_pHead;
            node->pPrev = nullptr;
        }
        Link(node);
        return reinterpret_cast<POSITION>(node);
    }
    POSITION InsertAfter(POSITION position, ARG_TYPE newElement) {
        CNode* at = reinterpret_cast<CNode*>(position);
        CNode* node = new CNode{at ? at->pNext : m_pHead, at, newElement};
        if (!at) {
            node->pNext = nullptr;
            node->pPrev = m_pTail;
        }
        Link(node);
        return reinterpret_cast<POSITION>(node);
    }
    POSITION Find(ARG_TYPE searchValue, POSITION startAfter = nullptr) const {
        CNode* n = startAfter ? reinterpret_cast<CNode*>(startAfter)->pNext : m_pHead;
        for (; n; n = n->pNext)
            if (n->data == searchValue)
                return reinterpret_cast<POSITION>(n);
        return nullptr;
    }
    POSITION FindIndex(INT_PTR nIndex) const {
        if (nIndex < 0 || nIndex >= m_nCount)
            return nullptr;
        CNode* n = m_pHead;
        while (nIndex-- > 0)
            n = n->pNext;
        return reinterpret_cast<POSITION>(n);
    }
    void Serialize(CArchive& ar) override;

private:
    void Link(CNode* node) {
        if (node->pPrev)
            node->pPrev->pNext = node;
        else
            m_pHead = node;
        if (node->pNext)
            node->pNext->pPrev = node;
        else
            m_pTail = node;
        ++m_nCount;
    }
    void Unlink(CNode* node) {
        if (node->pPrev)
            node->pPrev->pNext = node->pNext;
        else
            m_pHead = node->pNext;
        if (node->pNext)
            node->pNext->pPrev = node->pPrev;
        else
            m_pTail = node->pPrev;
        --m_nCount;
        delete node;
    }

    CNode* m_pHead = nullptr;
    CNode* m_pTail = nullptr;
    INT_PTR m_nCount = 0;
};

template <class BASE_CLASS, class TYPE>
class CTypedPtrList : public BASE_CLASS {
public:
    explicit CTypedPtrList(INT_PTR nBlockSize = 10) : BASE_CLASS(nBlockSize) {}
    TYPE& GetHead() { return reinterpret_cast<TYPE&>(BASE_CLASS::GetHead()); }
    TYPE GetHead() const { return static_cast<TYPE>(BASE_CLASS::GetHead()); }
    TYPE& GetTail() { return reinterpret_cast<TYPE&>(BASE_CLASS::GetTail()); }
    TYPE GetTail() const { return static_cast<TYPE>(BASE_CLASS::GetTail()); }
    TYPE RemoveHead() { return static_cast<TYPE>(BASE_CLASS::RemoveHead()); }
    TYPE RemoveTail() { return static_cast<TYPE>(BASE_CLASS::RemoveTail()); }
    TYPE& GetNext(POSITION& rPosition) { return reinterpret_cast<TYPE&>(BASE_CLASS::GetNext(rPosition)); }
    TYPE GetNext(POSITION& rPosition) const { return static_cast<TYPE>(BASE_CLASS::GetNext(rPosition)); }
    TYPE& GetPrev(POSITION& rPosition) { return reinterpret_cast<TYPE&>(BASE_CLASS::GetPrev(rPosition)); }
    TYPE GetPrev(POSITION& rPosition) const { return static_cast<TYPE>(BASE_CLASS::GetPrev(rPosition)); }
    TYPE& GetAt(POSITION position) { return reinterpret_cast<TYPE&>(BASE_CLASS::GetAt(position)); }
    TYPE GetAt(POSITION position) const { return static_cast<TYPE>(BASE_CLASS::GetAt(position)); }
    void SetAt(POSITION pos, TYPE newElement) { BASE_CLASS::SetAt(pos, newElement); }
    POSITION AddHead(TYPE newElement) { return BASE_CLASS::AddHead(newElement); }
    POSITION AddTail(TYPE newElement) { return BASE_CLASS::AddTail(newElement); }
};

template <class KEY>
struct CMapKeyLess {
    bool operator()(const KEY& a, const KEY& b) const { return a < b; }
};

template <class KEY, class ARG_KEY, class VALUE, class ARG_VALUE>
class CMap : public CObject {
    struct CAssoc {
        CAssoc* pNext;
        CAssoc* pPrev;
        KEY key;
        VALUE value;
    };

public:
    explicit CMap(INT_PTR = 10) {}
    CMap(const CMap&) = delete;
    CMap& operator=(const CMap&) = delete;
    ~CMap() override { RemoveAll(); }

    INT_PTR GetCount() const { return static_cast<INT_PTR>(m_index.size()); }
    INT_PTR GetSize() const { return GetCount(); }
    BOOL IsEmpty() const { return m_index.empty(); }
    BOOL Lookup(ARG_KEY key, VALUE& rValue) const {
        auto it = m_index.find(KEY(key));
        if (it == m_index.end())
            return FALSE;
        rValue = it->second->value;
        return TRUE;
    }
    VALUE& operator[](ARG_KEY key) {
        KEY k(key);
        auto it = m_index.find(k);
        if (it != m_index.end())
            return it->second->value;
        CAssoc* a = new CAssoc{nullptr, m_pTail, k, VALUE()};
        if (m_pTail)
            m_pTail->pNext = a;
        else
            m_pHead = a;
        m_pTail = a;
        m_index.emplace(k, a);
        return a->value;
    }
    void SetAt(ARG_KEY key, ARG_VALUE newValue) { (*this)[key] = newValue; }
    BOOL RemoveKey(ARG_KEY key) {
        auto it = m_index.find(KEY(key));
        if (it == m_index.end())
            return FALSE;
        CAssoc* a = it->second;
        m_index.erase(it);
        if (a->pPrev)
            a->pPrev->pNext = a->pNext;
        else
            m_pHead = a->pNext;
        if (a->pNext)
            a->pNext->pPrev = a->pPrev;
        else
            m_pTail = a->pPrev;
        delete a;
        return TRUE;
    }
    void RemoveAll() {
        while (m_pHead) {
            CAssoc* next = m_pHead->pNext;
            delete m_pHead;
            m_pHead = next;
        }
        m_pTail = nullptr;
        m_index.clear();
    }
    POSITION GetStartPosition() const { return reinterpret_cast<POSITION>(m_pHead); }
    void GetNextAssoc(POSITION& rNextPosition, KEY& rKey, VALUE& rValue) const {
        CAssoc* a = reinterpret_cast<CAssoc*>(rNextPosition);
        rKey = a->key;
        rValue = a->value;
        rNextPosition = reinterpret_cast<POSITION>(a->pNext);
    }
    void InitHashTable(UINT, BOOL = TRUE) {}
    UINT GetHashTableSize() const { return 17; }

private:
    CAssoc* m_pHead = nullptr;
    CAssoc* m_pTail = nullptr;
    std::map<KEY, CAssoc*, CMapKeyLess<KEY>> m_index;
};

template <class BASE_CLASS, class KEY, class VALUE>
class CTypedPtrMap : public BASE_CLASS {
public:
    BOOL Lookup(typename BASE_CLASS::BASE_ARG_KEY key, VALUE& rValue) const {
        typename BASE_CLASS::BASE_VALUE v;
        BOOL found = BASE_CLASS::Lookup(key, v);
        rValue = static_cast<VALUE>(v);
        return found;
    }
    VALUE& operator[](typename BASE_CLASS::BASE_ARG_KEY key) { return reinterpret_cast<VALUE&>(BASE_CLASS::operator[](key)); }
    void SetAt(KEY key, VALUE newValue) { BASE_CLASS::SetAt(key, newValue); }
    BOOL RemoveKey(KEY key) { return BASE_CLASS::RemoveKey(key); }
    void GetNextAssoc(POSITION& rPosition, KEY& rKey, VALUE& rValue) const {
        typename BASE_CLASS::BASE_KEY k;
        typename BASE_CLASS::BASE_VALUE v;
        BASE_CLASS::GetNextAssoc(rPosition, k, v);
        rKey = static_cast<KEY>(k);
        rValue = static_cast<VALUE>(v);
    }
};

#define MFCWX_DECLARE_ARRAY(name, TYPE, ARG) \
    class name : public CArray<TYPE, ARG> { \
        DECLARE_SERIAL(name) \
    public: \
        name() = default; \
        name(const name& o) : CArray<TYPE, ARG>(o) {} \
        name& operator=(const name& o) { CArray<TYPE, ARG>::operator=(o); return *this; } \
    };

MFCWX_DECLARE_ARRAY(CStringArray, CString, const char*)
MFCWX_DECLARE_ARRAY(CPtrArray, void*, void*)
MFCWX_DECLARE_ARRAY(CObArray, CObject*, CObject*)
MFCWX_DECLARE_ARRAY(CDWordArray, DWORD, DWORD)
MFCWX_DECLARE_ARRAY(CUIntArray, UINT, UINT)
MFCWX_DECLARE_ARRAY(CWordArray, WORD, WORD)
MFCWX_DECLARE_ARRAY(CByteArray, BYTE, BYTE)

#define MFCWX_DECLARE_LIST(name, TYPE, ARG) \
    class name : public CList<TYPE, ARG> { \
        DECLARE_SERIAL(name) \
    public: \
        explicit name(INT_PTR nBlockSize = 10) : CList<TYPE, ARG>(nBlockSize) {} \
    };

MFCWX_DECLARE_LIST(CStringList, CString, const char*)
MFCWX_DECLARE_LIST(CPtrList, void*, void*)
MFCWX_DECLARE_LIST(CObList, CObject*, CObject*)

#define MFCWX_DECLARE_MAP(name, K, AK, V, AV) \
    class name : public CMap<K, AK, V, AV> { \
        DECLARE_SERIAL(name) \
    public: \
        typedef K BASE_KEY; \
        typedef AK BASE_ARG_KEY; \
        typedef V BASE_VALUE; \
        typedef AV BASE_ARG_VALUE; \
        explicit name(INT_PTR nBlockSize = 10) : CMap<K, AK, V, AV>(nBlockSize) {} \
    };

MFCWX_DECLARE_MAP(CMapStringToString, CString, const char*, CString, const char*)
MFCWX_DECLARE_MAP(CMapStringToPtr, CString, const char*, void*, void*)
MFCWX_DECLARE_MAP(CMapStringToOb, CString, const char*, CObject*, CObject*)
MFCWX_DECLARE_MAP(CMapPtrToPtr, void*, void*, void*, void*)
MFCWX_DECLARE_MAP(CMapPtrToWord, void*, void*, WORD, WORD)
MFCWX_DECLARE_MAP(CMapWordToPtr, WORD, WORD, void*, void*)
MFCWX_DECLARE_MAP(CMapWordToOb, WORD, WORD, CObject*, CObject*)

template <class TYPE>
void SerializeElements(CArchive& ar, TYPE* pElements, INT_PTR nCount) {
    if (ar.IsStoring())
        ar.Write(pElements, static_cast<UINT>(nCount * sizeof(TYPE)));
    else
        ar.Read(pElements, static_cast<UINT>(nCount * sizeof(TYPE)));
}

inline void SerializeElements(CArchive& ar, CString* pElements, INT_PTR nCount) {
    for (INT_PTR i = 0; i < nCount; ++i) {
        if (ar.IsStoring())
            ar << pElements[i];
        else
            ar >> pElements[i];
    }
}

template <class TYPE, class ARG_TYPE>
void CArray<TYPE, ARG_TYPE>::Serialize(CArchive& ar) {
    if (ar.IsStoring()) {
        ar.WriteCount(static_cast<UINT>(GetSize()));
    } else {
        SetSize(ar.ReadCount());
    }
    if (!m_data.empty())
        SerializeElements(ar, m_data.data(), GetSize());
}

template <class TYPE, class ARG_TYPE>
void CList<TYPE, ARG_TYPE>::Serialize(CArchive& ar) {
    if (ar.IsStoring()) {
        ar.WriteCount(static_cast<UINT>(m_nCount));
        for (CNode* n = m_pHead; n; n = n->pNext)
            SerializeElements(ar, &n->data, 1);
    } else {
        UINT count = ar.ReadCount();
        while (count--) {
            TYPE value{};
            SerializeElements(ar, &value, 1);
            AddTail(value);
        }
    }
}
