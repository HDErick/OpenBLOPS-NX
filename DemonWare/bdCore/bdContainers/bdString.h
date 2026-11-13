#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
class bdString
{
public:
    explicit bdString(const char *value);
    bdString(const bdString &other);
    ~bdString();
    bdString &operator=(const bdString &other);
    const char *getBuffer() const { return m_string; }

private:
    struct Storage
    {
        unsigned int references;
        unsigned int length;
        unsigned int capacity;
        char text[1];
    };
    static Storage s_empty;
    Storage *storage() const;
    void release();
    char *m_string;
};
static_assert(sizeof(void *) != 4 || sizeof(bdString) == 4);
#endif
