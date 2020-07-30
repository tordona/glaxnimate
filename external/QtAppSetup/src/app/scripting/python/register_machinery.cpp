#include "register_machinery.hpp"

#include <utility>

#include <QColor>
#include <QUuid>
#include <QVariant>


namespace app::scripting::python {

template<class T> const char* type_name();
template<int> struct meta_2_cpp_s;
template<class> struct cpp_2_meta_s;

#define TYPE_NAME(Type) template<> const char* type_name<Type>() { return #Type; }
#define SETUP_TYPE(MetaInt, Type)                                   \
    template<> const char* type_name<Type>() { return #Type; }      \
    template<> struct meta_2_cpp_s<MetaInt> { using type = Type; }; \
    template<> struct cpp_2_meta_s<Type> { static constexpr const int value = MetaInt; };

template<int meta_type> using meta_2_cpp = typename meta_2_cpp_s<meta_type>::type;
template<class T> constexpr const int cpp_2_meta = cpp_2_meta_s<T>::value;

SETUP_TYPE(QMetaType::Int,          int)
SETUP_TYPE(QMetaType::Bool,         bool)
SETUP_TYPE(QMetaType::Double,       double)
SETUP_TYPE(QMetaType::Float,        float)
SETUP_TYPE(QMetaType::UInt,         unsigned int)
SETUP_TYPE(QMetaType::Long,         long)
SETUP_TYPE(QMetaType::LongLong,     long long)
SETUP_TYPE(QMetaType::Short,        short)
SETUP_TYPE(QMetaType::ULong,        unsigned long)
SETUP_TYPE(QMetaType::ULongLong,    unsigned long long)
SETUP_TYPE(QMetaType::UShort,       unsigned short)
SETUP_TYPE(QMetaType::QString,      QString)
SETUP_TYPE(QMetaType::QColor,       QColor)
SETUP_TYPE(QMetaType::QUuid,        QUuid)
SETUP_TYPE(QMetaType::QObjectStar,  QObject*)
SETUP_TYPE(QMetaType::QVariantList, QVariantList)
SETUP_TYPE(QMetaType::QVariant,     QVariant)
SETUP_TYPE(QMetaType::QStringList,  QStringList)
SETUP_TYPE(QMetaType::QVariantMap,  QVariantMap)
SETUP_TYPE(QMetaType::QVariantHash, QVariantHash)
// If you add stuff here, remember to add it to supported_types too

TYPE_NAME(std::vector<QObject*>)

using supported_types = std::integer_sequence<int,
    QMetaType::Bool,
    QMetaType::Int,
    QMetaType::Double,
    QMetaType::Float,
    QMetaType::UInt,
    QMetaType::Long,
    QMetaType::LongLong,
    QMetaType::Short,
    QMetaType::ULong,
    QMetaType::ULongLong,
    QMetaType::UShort,
    QMetaType::QString,
    QMetaType::QColor,
    QMetaType::QUuid,
    QMetaType::QObjectStar,
    QMetaType::QVariantList,
    QMetaType::QVariant,
    QMetaType::QStringList,
    QMetaType::QVariantMap,
    QMetaType::QVariantHash
    // Ensure new types have SETUP_TYPE
>;

template<class T> QVariant qvariant_from_cpp(const T& t) { return QVariant::fromValue(t); }
template<class T> T qvariant_to_cpp(const QVariant& v) { return v.value<T>(); }

template<> QVariant qvariant_from_cpp<std::string>(const std::string& t) { return QString::fromStdString(t); }
template<> std::string qvariant_to_cpp<std::string>(const QVariant& v) { return v.toString().toStdString(); }

template<> QVariant qvariant_from_cpp<QVariant>(const QVariant& t) { return t; }
template<> QVariant qvariant_to_cpp<QVariant>(const QVariant& v) { return v; }


template<> void qvariant_to_cpp<void>(const QVariant&) {}


template<>
QVariant qvariant_from_cpp<std::vector<QObject*>>(const std::vector<QObject*>& t)
{
    QVariantList list;
    for ( QObject* obj : t )
        list.push_back(QVariant::fromValue(obj));
    return list;

}
template<> std::vector<QObject*> qvariant_to_cpp<std::vector<QObject*>>(const QVariant& v)
{
    std::vector<QObject*> objects;
    for ( const QVariant& vi : v.toList() )
        objects.push_back(vi.value<QObject*>());
    return objects;
}

template<int i, template<class FuncT> class Func, class RetT, class... FuncArgs>
bool type_dispatch_impl_step(int meta_type, RetT& ret, FuncArgs&&... args)
{
    if ( meta_type != i )
        return false;

    ret = Func<meta_2_cpp<i>>::do_the_thing(std::forward<FuncArgs>(args)...);
    return true;
}

template<template<class FuncT> class Func, class RetT, class... FuncArgs, int... i>
bool type_dispatch_impl(int meta_type, RetT& ret, std::integer_sequence<int, i...>, FuncArgs&&... args)
{
    return (type_dispatch_impl_step<i, Func>(meta_type, ret, std::forward<FuncArgs>(args)...)||...);
}

template<template<class FuncT> class Func, class RetT, class... FuncArgs>
RetT type_dispatch(int meta_type, FuncArgs&&... args)
{
    if ( meta_type >= QMetaType::User )
    {
        return Func<QObject*>::do_the_thing(std::forward<FuncArgs>(args)...);
    }
    RetT ret;
    type_dispatch_impl<Func>(meta_type, ret, supported_types(), std::forward<FuncArgs>(args)...);
    return ret;
}

template<template<class FuncT> class Func, class RetT, class... FuncArgs>
static RetT type_dispatch_maybe_void(int meta_type, FuncArgs&&... args)
{
    if ( meta_type == QMetaType::Void )
        return Func<void>::do_the_thing(std::forward<FuncArgs>(args)...);
    return type_dispatch<Func, RetT>(meta_type, std::forward<FuncArgs>(args)...);
}


template<class CppType>
    struct RegisterProperty
    {
        static PyPropertyInfo do_the_thing(const QMetaProperty& prop)
        {
            PyPropertyInfo py;
            py.name = prop.name();
            py.get = py::cpp_function(
                [prop](const QObject* o) { return qvariant_to_cpp<CppType>(prop.read(o)); },
                py::return_value_policy::automatic_reference
            );

            if ( prop.isWritable() )
                py.set = py::cpp_function([prop](QObject* o, const CppType& v) {
                    prop.write(o, qvariant_from_cpp<CppType>(v));
                });
            return py;
        }
    };

PyPropertyInfo register_property(const QMetaProperty& prop)
{
    if ( !prop.isScriptable() )
        return {};

    PyPropertyInfo pyprop = type_dispatch<RegisterProperty, PyPropertyInfo>(prop.type(), prop);
    if ( !pyprop.name )
        qWarning() << "Invalid property" << prop.name() << "of type" << prop.type() << prop.typeName();
    return pyprop;
}



class ArgumentBuffer
{
public:
    ArgumentBuffer() = default;
    ArgumentBuffer(const ArgumentBuffer&) = delete;
    ArgumentBuffer& operator=(const ArgumentBuffer&) = delete;
    ~ArgumentBuffer()
    {
        for ( const auto& d : destructors )
            d();
    }

    template<class CppType>
    CppType* allocate()
    {
        if ( avail() < int(sizeof(CppType)) )
            throw py::type_error("Cannot allocate argument");

        CppType* addr = new (next_mem()) CppType;
        buffer_used += sizeof(CppType);
        generic_args[arguments] = { type_name<CppType>(), addr };
        ensure_destruction(addr);
        arguments += 1;
        return addr;
    }

    template<class CppType>
    void allocate_return_type()
    {
        if ( avail() < int(sizeof(CppType)) )
            throw py::type_error("Cannot allocate return value");

        CppType* addr = new (next_mem()) CppType;
        buffer_used += sizeof(CppType);
        ret = { type_name<CppType>(), addr };
        ensure_destruction(addr);
        ret_addr = addr;
    }

    template<class CppType>
    CppType return_value()
    {
        return *static_cast<CppType*>(ret_addr);
    }

    const QGenericArgument& arg(int i) const { return generic_args[i]; }

    const QGenericReturnArgument& return_arg() const { return ret; }

private:
    int arguments = 0;
    int buffer_used = 0;
    std::array<char, 128> buffer;
    std::vector<std::function<void()>> destructors;
    std::array<QGenericArgument, 9> generic_args;
    QGenericReturnArgument ret;
    void* ret_addr = nullptr;


    int avail() { return buffer.size() - buffer_used; }
    void* next_mem() { return &buffer + buffer_used; }


    template<class CppType>
        std::enable_if_t<std::is_pod_v<CppType>> ensure_destruction(CppType*) {}


    template<class CppType>
        std::enable_if_t<!std::is_pod_v<CppType>> ensure_destruction(CppType* addr)
        {
           destructors.push_back([addr]{ addr->~CppType(); });
        }
};

template<> void ArgumentBuffer::allocate_return_type<void>(){}
template<> void ArgumentBuffer::return_value<void>(){}


template<class CppType>
    struct ConvertArgument
    {
        static bool do_the_thing(const py::handle& val, ArgumentBuffer& buf)
        {
            *buf.allocate<CppType>() = val.cast<CppType>();
            return true;
        }
    };

bool convert_argument(int meta_type, const py::handle& value, ArgumentBuffer& buf)
{
    return type_dispatch<ConvertArgument, bool>(meta_type, value, buf);
}


template<class ReturnType>
struct RegisterMethod
{
    static PyMethodInfo do_the_thing(const QMetaMethod& meth, py::handle& handle)
    {
        PyMethodInfo py;
        py.name = meth.name();
        py.method = py::cpp_function(
            [meth](QObject* o, py::args args)
            {
                int len = py::len(args);
                if ( len > 9 )
                    throw pybind11::value_error("Invalid argument count");

                ArgumentBuffer argbuf;

                argbuf.allocate_return_type<ReturnType>();

                for ( int i = 0; i < len; i++ )
                {
                   if ( !convert_argument(meth.parameterType(i), args[i], argbuf) )
                        throw pybind11::value_error("Invalid argument");
                }

                // Calling by name from QMetaObject ensures that default arguments work correctly
                bool ok = QMetaObject::invokeMethod(
                    o,
                    meth.name(),
                    Qt::DirectConnection,
                    argbuf.return_arg(),
                    argbuf.arg(0),
                    argbuf.arg(1),
                    argbuf.arg(2),
                    argbuf.arg(3),
                    argbuf.arg(4),
                    argbuf.arg(5),
                    argbuf.arg(6),
                    argbuf.arg(7),
                    argbuf.arg(8),
                    argbuf.arg(9)
                );
                if ( !ok )
                    throw pybind11::value_error("Invalid method invocation");
                return argbuf.return_value<ReturnType>();
            },
            py::name(py.name),
            py::is_method(handle),
            py::sibling(py::getattr(handle, py.name, py::none())),
            py::return_value_policy::automatic_reference
        );
        return py;
    }
};

PyMethodInfo register_method(const QMetaMethod& meth, py::handle& handle)
{
    if ( meth.access() != QMetaMethod::Public )
        return {};
    if ( meth.methodType() != QMetaMethod::Method && meth.methodType() != QMetaMethod::Slot )
        return {};

    if ( meth.parameterCount() > 9 )
    {
        qDebug() << "Too many arguments for method " << meth.name() << ": " << meth.parameterCount();
        return {};
    }

    PyMethodInfo pymeth = type_dispatch_maybe_void<RegisterMethod, PyMethodInfo>(meth.returnType(), meth, handle);
    if ( !pymeth.name )
        qWarning() << "Invalid method" << meth.name() << "return type" << meth.returnType() << meth.typeName();
    return pymeth;

}

template<int i>
bool qvariant_type_caster_load_impl(QVariant& into, const pybind11::handle& src)
{
    auto caster = pybind11::detail::make_caster<meta_2_cpp<i>>();
    if ( caster.load(src, false) )
    {
        into = QVariant::fromValue(pybind11::detail::cast_op<meta_2_cpp<i>>(caster));
        return true;
    }
    return false;
}

template<>
bool qvariant_type_caster_load_impl<QMetaType::QVariant>(QVariant&, const pybind11::handle&) { return false; }

template<int... i>
bool qvariant_type_caster_load(QVariant& into, const pybind11::handle& src, std::integer_sequence<int, i...>)
{
    return (qvariant_type_caster_load_impl<i>(into, src)||...);
}

template<int i>
bool qvariant_type_caster_cast_impl(
    pybind11::handle& into, const QVariant& src,
    pybind11::return_value_policy policy, const pybind11::handle& parent)
{
    if ( src.type() == i )
    {
        into = pybind11::detail::make_caster<meta_2_cpp<i>>::cast(src.value<meta_2_cpp<i>>(), policy, parent);
        return true;
    }
    return false;
}

template<>
bool qvariant_type_caster_cast_impl<QMetaType::QVariant>(
    pybind11::handle&, const QVariant&, pybind11::return_value_policy, const pybind11::handle&)
{
    return false;
}


template<int... i>
bool qvariant_type_caster_cast(
    pybind11::handle& into,
    const QVariant& src,
    pybind11::return_value_policy policy,
    const pybind11::handle& parent,
    std::integer_sequence<int, i...>
)
{
    return (qvariant_type_caster_cast_impl<i>(into, src, policy, parent)||...);
}


} // namespace app::scripting::python

using namespace app::scripting::python;

bool pybind11::detail::type_caster<QVariant>::load(handle src, bool)
{
    if ( src.ptr() == Py_None )
    {
        value = QVariant();
        return true;
    }
    return qvariant_type_caster_load(value, src, supported_types());
}

pybind11::handle pybind11::detail::type_caster<QVariant>::cast(QVariant src, return_value_policy policy, handle parent)
{
    if ( src.isNull() )
        return pybind11::none();

    policy = py::return_value_policy::automatic_reference;

    if ( src.type() == QVariant::UserType )
        return pybind11::detail::make_caster<QObject*>::cast(src.value<QObject*>(), policy, parent);

    pybind11::handle ret;
    qvariant_type_caster_cast(ret, src, policy, parent, supported_types());
    return ret;
}