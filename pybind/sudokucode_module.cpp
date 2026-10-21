#include <Python.h>
#include "src/sudokucode.h"

namespace {

PyObject* PyEncode(PyObject* self, PyObject* args) {
    PyObject* dataObj;
    int n = 9, ecc = 2;
    int autoEcc = 0, encrypt = 0, compress = 0;
    const char* key = "";
    const char* passphrase = "";
    int verifyOnly = 0;

    if (!PyArg_ParseTuple(args, "O|iiiiipp", &dataObj, &n, &ecc, &autoEcc,
                          &encrypt, &key, &passphrase, &compress, &verifyOnly))
        return nullptr;

    if (!PyBytes_Check(dataObj)) {
        PyErr_SetString(PyExc_TypeError, "data must be bytes");
        return nullptr;
    }

    Py_ssize_t len = PyBytes_Size(dataObj);
    const char* data = PyBytes_AsString(dataObj);
    std::vector<uint8_t> vec(data, data + len);

    Params p;
    p.n = n;
    p.eccLevel = ecc;
    p.autoEcc = autoEcc != 0;
    p.encrypt = encrypt != 0;
    p.key = key ? key : "";
    p.passphrase = passphrase ? passphrase : "";
    p.compress = compress != 0;

    try {
        auto pages = encode(vec, p);
        PyObject* list = PyList_New(pages.size());
        for (size_t i = 0; i < pages.size(); ++i) {
            PyObject* dict = PyDict_New();
            PyDict_SetItemString(dict, "n", PyLong_FromLong(pages[i].n));
            PyDict_SetItemString(dict, "page", PyLong_FromUnsignedLong(pages[i].page));
            PyDict_SetItemString(dict, "total_pages",
                                 PyLong_FromUnsignedLong(pages[i].totalPages));
            PyDict_SetItemString(dict, "length",
                                 PyLong_FromUnsignedLong(pages[i].length));
            PyDict_SetItemString(dict, "crc",
                                 PyLong_FromUnsignedLong(pages[i].crc));
            PyObject* grid = PyBytes_FromStringAndSize(
                reinterpret_cast<const char*>(pages[i].grid.data()),
                pages[i].grid.size());
            PyDict_SetItemString(dict, "grid", grid);
            Py_DECREF(grid);
            PyObject* mask = PyBytes_FromStringAndSize(
                reinterpret_cast<const char*>(pages[i].mask.data()),
                pages[i].mask.size());
            PyDict_SetItemString(dict, "mask", mask);
            Py_DECREF(mask);
            PyList_SetItem(list, i, dict);
        }
        return list;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

PyObject* PyDecode(PyObject* self, PyObject* args) {
    PyObject* pagesObj;
    int n = 9, ecc = 2;
    int autoEcc = 0, encrypt = 0, compress = 0;
    const char* key = "";
    const char* passphrase = "";

    if (!PyArg_ParseTuple(args, "O|iiiipp", &pagesObj, &n, &ecc, &autoEcc,
                          &encrypt, &key, &passphrase, &compress))
        return nullptr;

    if (!PyList_Check(pagesObj)) {
        PyErr_SetString(PyExc_TypeError, "pages must be a list");
        return nullptr;
    }

    std::vector<Code> pages;
    Py_ssize_t count = PyList_Size(pagesObj);
    for (Py_ssize_t i = 0; i < count; ++i) {
        PyObject* item = PyList_GetItem(pagesObj, i);
        if (!PyDict_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "each page must be a dict");
            return nullptr;
        }
        Code c;
        PyObject* tmp;
        tmp = PyDict_GetItemString(item, "n");
        c.n = tmp ? PyLong_AsLong(tmp) : 9;
        tmp = PyDict_GetItemString(item, "page");
        c.page = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 0;
        tmp = PyDict_GetItemString(item, "total_pages");
        c.totalPages = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 1;
        tmp = PyDict_GetItemString(item, "length");
        c.length = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 0;
        tmp = PyDict_GetItemString(item, "crc");
        c.crc = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 0;
        tmp = PyDict_GetItemString(item, "grid");
        if (tmp && PyBytes_Check(tmp)) {
            Py_ssize_t glen = PyBytes_Size(tmp);
            const char* gdata = PyBytes_AsString(tmp);
            c.grid.assign(gdata, gdata + glen);
        }
        tmp = PyDict_GetItemString(item, "mask");
        if (tmp && PyBytes_Check(tmp)) {
            Py_ssize_t mlen = PyBytes_Size(tmp);
            const char* mdata = PyBytes_AsString(tmp);
            c.mask.assign(mdata, mdata + mlen);
        }
        pages.push_back(c);
    }

    Params p;
    p.n = n;
    p.eccLevel = ecc;
    p.autoEcc = autoEcc != 0;
    p.encrypt = encrypt != 0;
    p.key = key ? key : "";
    p.passphrase = passphrase ? passphrase : "";
    p.compress = compress != 0;

    try {
        auto data = decode(pages, p);
        return PyBytes_FromStringAndSize(
            reinterpret_cast<const char*>(data.data()), data.size());
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

PyObject* PyVerify(PyObject* self, PyObject* args) {
    PyObject* pagesObj;
    int n = 9, ecc = 2;
    int autoEcc = 0, encrypt = 0, compress = 0;
    const char* key = "";
    const char* passphrase = "";

    if (!PyArg_ParseTuple(args, "O|iiiipp", &pagesObj, &n, &ecc, &autoEcc,
                          &encrypt, &key, &passphrase, &compress))
        return nullptr;

    if (!PyList_Check(pagesObj)) {
        PyErr_SetString(PyExc_TypeError, "pages must be a list");
        return nullptr;
    }

    std::vector<Code> pages;
    Py_ssize_t count = PyList_Size(pagesObj);
    for (Py_ssize_t i = 0; i < count; ++i) {
        PyObject* item = PyList_GetItem(pagesObj, i);
        Code c;
        PyObject* tmp = PyDict_GetItemString(item, "n");
        c.n = tmp ? PyLong_AsLong(tmp) : 9;
        tmp = PyDict_GetItemString(item, "page");
        c.page = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 0;
        tmp = PyDict_GetItemString(item, "total_pages");
        c.totalPages = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 1;
        tmp = PyDict_GetItemString(item, "length");
        c.length = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 0;
        tmp = PyDict_GetItemString(item, "crc");
        c.crc = tmp ? static_cast<uint32_t>(PyLong_AsUnsignedLong(tmp)) : 0;
        tmp = PyDict_GetItemString(item, "grid");
        if (tmp && PyBytes_Check(tmp)) {
            Py_ssize_t glen = PyBytes_Size(tmp);
            c.grid.assign(PyBytes_AsString(tmp), PyBytes_AsString(tmp) + glen);
        }
        tmp = PyDict_GetItemString(item, "mask");
        if (tmp && PyBytes_Check(tmp)) {
            Py_ssize_t mlen = PyBytes_Size(tmp);
            c.mask.assign(PyBytes_AsString(tmp), PyBytes_AsString(tmp) + mlen);
        }
        pages.push_back(c);
    }

    Params p;
    p.n = n;
    p.eccLevel = ecc;
    p.autoEcc = autoEcc != 0;
    p.encrypt = encrypt != 0;
    p.key = key ? key : "";
    p.passphrase = passphrase ? passphrase : "";
    p.compress = compress != 0;

    bool ok = verify(pages, p);
    return PyBool_FromLong(ok ? 1 : 0);
}

PyObject* PyDeriveKey(PyObject* self, PyObject* args) {
    const char* passphrase;
    const char* salt = "SudokuCode";
    int iterations = 10000;
    if (!PyArg_ParseTuple(args, "s|si", &passphrase, &salt, &iterations))
        return nullptr;
    std::string key = deriveKey(passphrase, salt, iterations);
    if (key.empty()) {
        PyErr_SetString(PyExc_RuntimeError, "key derivation failed");
        return nullptr;
    }
    return PyBytes_FromStringAndSize(key.data(), key.size());
}

static PyMethodDef SudokuCodeMethods[] = {
    {"encode", PyEncode, METH_VARARGS,
     "encode(data, n=9, ecc=2, auto_ecc=False, encrypt=False, key='', passphrase='', compress=False) -> list[dict]"},
    {"decode", PyDecode, METH_VARARGS,
     "decode(pages, n=9, ecc=2, auto_ecc=False, encrypt=False, key='', passphrase='', compress=False) -> bytes"},
    {"verify", PyVerify, METH_VARARGS,
     "verify(pages, n=9, ecc=2, auto_ecc=False, encrypt=False, key='', passphrase='', compress=False) -> bool"},
    {"derive_key", PyDeriveKey, METH_VARARGS,
     "derive_key(passphrase, salt='SudokuCode', iterations=10000) -> bytes"},
    {nullptr, nullptr, 0, nullptr}
};

static struct PyModuleDef sudokucodeModule = {
    PyModuleDef_HEAD_INIT,
    "sudokucode",
    "SudokuCode: Sudoku-based 2D barcode encoding",
    -1,
    SudokuCodeMethods
};

PyMODINIT_FUNC PyInit_sudokucode(void) {
    return PyModule_Create(&sudokucodeModule);
}

} // namespace
