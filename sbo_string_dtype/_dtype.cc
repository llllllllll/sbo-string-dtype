#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

namespace sbo_string_dtype {
class sbo_string {
private:
    constexpr static std::size_t sb_size = sizeof(char*);
    using sb_type = std::array<char, sb_size>;

    std::size_t m_size;
    union {
        char* as_ptr;
        sb_type as_arr;
    } m_data;

public:
    sbo_string() : m_size(0) {}

    sbo_string(sbo_string&& mvfrom) noexcept
        : m_size(mvfrom.m_size), m_data(mvfrom.m_data) {
        mvfrom.m_size = 0;
    }

    sbo_string(const sbo_string& cpfrom) : sbo_string(cpfrom.view()) {}

    explicit sbo_string(std::string_view v) : m_size(v.size()) {
        char* ptr;
        if (m_size > sb_size) {
            ptr = m_data.as_ptr = new char[m_size];
        }
        else {
            new (&m_data.as_arr) sb_type{'\0'};
            ptr = m_data.as_arr.data();
        }
        std::memcpy(ptr, v.data(), m_size);
    }

    void swap(sbo_string& other) noexcept {
        std::swap(m_size, other.m_size);
        std::swap(m_data, other.m_data);
    }

    sbo_string& operator=(std::string_view v) {
        if (v.data() == data()) {
            return *this;
        }

        sbo_string tmp{v};
        swap(tmp);
        return *this;
    }

    sbo_string& operator=(const sbo_string& cpfrom) {
        return operator=(cpfrom.view());
    }

    sbo_string& operator=(sbo_string&& mvfrom) noexcept {
        if (mvfrom.data() == data()) {
            return *this;
        }

        swap(mvfrom);
        return *this;
    }

    ~sbo_string() {
        if (m_size > sb_size) {
            delete[] m_data.as_ptr;
        }
    }

    std::size_t size() const noexcept {
        return m_size;
    }

    char* data() noexcept {
        if (m_size > sb_size) {
            return m_data.as_ptr;
        }
        return m_data.as_arr.data();
    }

    const char* data() const noexcept {
        if (size() > sb_size) {
            return m_data.as_ptr;
        }
        return m_data.as_arr.data();
    }

    std::string_view view() const {
        return {data(), size()};
    }

    operator std::string_view() const {
        return view();
    }
};

namespace ops {
PyObject* getitem(void* data, void*) {
    auto* val = reinterpret_cast<sbo_string*>(data);
    return PyBytes_FromStringAndSize(val->view().data(), val->size());
}

int setitem(PyObject* item, void* data, void*) {
    char* cs;
    Py_ssize_t size;
    if (PyBytes_AsStringAndSize(item, &cs, &size)) {
        return 1;
    }

    auto* val = reinterpret_cast<sbo_string*>(data);
    val->~sbo_string();
    *val = sbo_string{std::string_view{cs, static_cast<std::size_t>(size)}};
    return 0;
}

void copyswapn(void* dst_,
               npy_intp dstride,
               void* src_,
               npy_intp sstride,
               npy_intp n,
               int,
               void*) {
    auto* src = reinterpret_cast<const char*>(src_);
    auto* dst = reinterpret_cast<char*>(dst_);
    if (!src) {
        return;
    }

    for (npy_intp ix = 0; ix < n; ++ix) {
        *reinterpret_cast<sbo_string*>(dst + dstride * ix) =
            *reinterpret_cast<const sbo_string*>(src + sstride * ix);
    }
}

void copyswap(void* dst_, void* src_, int, void*) {
    auto* src = reinterpret_cast<const char*>(src_);
    auto* dst = reinterpret_cast<char*>(dst_);
    if (!src) {
        return;
    }

    *reinterpret_cast<sbo_string*>(dst) = *reinterpret_cast<const sbo_string*>(src);
}

int compare(const void* lhs_, const void* rhs_, void*) {
    const auto& lhs = *reinterpret_cast<const sbo_string*>(lhs_);
    const auto& rhs = *reinterpret_cast<const sbo_string*>(rhs_);

    int res = lhs.view().compare(rhs);
    if (res < 0) {
        return -1;
    }
    else if (res > 0) {
        return 1;
    }
    return 0;
}

npy_bool nonzero(void* data, void*) {
    const auto& val = *reinterpret_cast<const sbo_string*>(data);
    return val.size();
}

int construct(void* data, npy_intp n, void*) {
    auto* arr = reinterpret_cast<sbo_string*>(data);
    npy_intp ix = 0;
    for (; ix < n; ++ix) {
        try {
            new(&arr[ix]) sbo_string{};
        }
        catch (const std::exception& e) {
            for (npy_intp unwind_ix = 0; unwind_ix < ix; ++unwind_ix) {
                arr[ix].~sbo_string();
            }
            PyErr_SetString(PyExc_RuntimeError, e.what());
            return -1;
        }
    }
    return 0;
}

void destruct(void* data, npy_intp n, void*) {
    auto* arr = reinterpret_cast<sbo_string*>(data);
    for (npy_intp ix = 0; ix < n; ++ix) {
        arr[ix].~sbo_string();
    }
}
}  // namespace ops

namespace npy {
PyArray_ArrFuncs arrfuncs;

PyArray_Descr descr = {
    // clang-format off
    PyObject_HEAD_INIT(0)
    // clang-format on
    &PyBytes_Type, /* typeobj */
    'V',           /* kind */
    'Z',           /* type */
    '|',           /* byteorder */
    /*
     * For now, we need NPY_NEEDS_PYAPI in order to make numpy detect our
     * exceptions.  This isn't technically necessary,
     * since we're careful about thread safety, and hopefully future
     * versions of numpy will recognize that.
     */
    NPY_NEEDS_PYAPI | NPY_USE_GETITEM | NPY_USE_SETITEM, /* flags */
    0,                                                   /* type_num */
    sizeof(sbo_string),                                  /* elsize */
    alignof(sbo_string),                                 /* alignment */
    0,                                                   /* subarray */
    0,                                                   /* fields */
    0,                                                   /* names */
    &arrfuncs,                                           /* f */
};
}  // namespace npy

namespace {
PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "sbo_string_dtype.dtype",
    nullptr,
    -1,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

PyMODINIT_FUNC PyInit__dtype() {
    import_array();

    PyArray_InitArrFuncs(&npy::arrfuncs);
    npy::arrfuncs.getitem = ops::getitem;
    npy::arrfuncs.setitem = ops::setitem;
    npy::arrfuncs.copyswapn = ops::copyswapn;
    npy::arrfuncs.copyswap = ops::copyswap;
    npy::arrfuncs.compare = ops::compare;
    npy::arrfuncs.nonzero = ops::nonzero;
    npy::arrfuncs.construct = ops::construct;
    npy::arrfuncs.destruct = ops::destruct;

    Py_TYPE(&npy::descr) = &PyArrayDescr_Type;

    if (PyArray_RegisterDataType(&npy::descr) < 0) {
        return nullptr;
    }

    PyObject* m = PyModule_Create(&module);
    if (!m) {
        return nullptr;
    }

    if (PyObject_SetAttrString(m, "dtype", reinterpret_cast<PyObject*>(&npy::descr))) {
        Py_DECREF(m);
        return nullptr;
    }

    return m;
}
}  // namespace
}  // namespace sbo_string_dtype
