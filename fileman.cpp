#include <fileman_p.hpp>

Q_LOGGING_CATEGORY(fileman, "FileMan")

////////////////////////////////////////////////////////////////////////////////////////////////////
// add_file_t //////////////////////////////////////////////////////////////////////////////////////

add_file_t::add_file_t() : d{ new FileManPrivateData } { }

add_file_t::add_file_t(int status, const QUrl &location, const QString &id)
    : d{ new FileManPrivateData }
{
    this->setId(id);
    this->setLocation(location);
    this->setStatus(status);
}

add_file_t::add_file_t(const add_file_t &other)     = default;
add_file_t::add_file_t(add_file_t &&other) noexcept = default;
add_file_t::~add_file_t()                           = default;

auto add_file_t::operator=(const add_file_t &other) -> add_file_t &     = default;
auto add_file_t::operator=(add_file_t &&other) noexcept -> add_file_t & = default;

auto add_file_t::id() const -> QString
{
    return (this->isNull()) ? QString{ } : this->d->m_id;
}

// True only after this object has been moved-from. A default-constructed add_file_t is never null.
auto add_file_t::isNull() const -> bool
{
    return (this->d == nullptr);
}

auto add_file_t::location() const -> QUrl
{
    return (this->isNull()) ? QUrl{ } : this->d->m_location;
}

auto add_file_t::status() const -> int
{
    return (this->isNull()) ? -1 : this->d->m_status;
}

auto add_file_t::setId(const QString &id) -> void
{
    if (!this->isNull())
        this->d->m_id = id;
}

auto add_file_t::setLocation(const QUrl &location) -> void
{
    if (!this->isNull())
        this->d->m_location = location;
}

auto add_file_t::setStatus(int status) -> void
{
    if (!this->isNull())
        this->d->m_status = status;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// rm_file_t ///////////////////////////////////////////////////////////////////////////////////////

rm_file_t::rm_file_t() : d{ new FileManPrivateData } { }

rm_file_t::rm_file_t(int status_l, int status_b) : d{ new FileManPrivateData }
{
    this->setStatus_b(status_b);
    this->setStatus_l(status_l);
}

rm_file_t::rm_file_t(const rm_file_t &other)     = default;
rm_file_t::rm_file_t(rm_file_t &&other) noexcept = default;
rm_file_t::~rm_file_t()                          = default;

auto rm_file_t::operator=(const rm_file_t &other) -> rm_file_t &     = default;
auto rm_file_t::operator=(rm_file_t &&other) noexcept -> rm_file_t & = default;

// True only after this object has been moved-from. A default-constructed rm_file_t is never null.
auto rm_file_t::isNull() const -> bool
{
    return this->d == nullptr;
}

auto rm_file_t::setStatus_b(int status_b) -> void
{
    if (this->isNull())
        this->d->m_status_b = status_b;
}

auto rm_file_t::setStatus_l(int status_l) -> void
{
    if (this->isNull())
        this->d->m_status_l = status_l;
}

auto rm_file_t::status_b() const -> int
{
    return (this->isNull()) ? -1 : this->d->m_status_b;
}

auto rm_file_t::status_l() const -> int
{
    return (this->isNull()) ? -1 : this->d->m_status_l;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileMan /////////////////////////////////////////////////////////////////////////////////////////

FileMan::FileMan(const QDir &storeDir, QObject *parent)
    : QObject{ parent }, d_ptr{ new FileManPrivate{ this } }
{
    if (FileMan::instanceExists())
        logFatal << "A second FileMan instance was constructed; only one instance may exist per "
                    "process";

    Q_D(FileMan);
    d->initPathsAndDicts(storeDir);
    d->p_storeLock =
            new QLockFile{ QDir{ d->m_storeDirPath }.absoluteFilePath(".fileman.lock"_L1) };

    // The singleton's lifetime is owned by this TU (main()/first construction), not by the
    // QQmlEngine. Make that explicit so the engine never attempts to garbage-collect or delete it.
    QQmlEngine::setObjectOwnership(FileMan::s_singletonInstance, QQmlEngine::CppOwnership);

    FileMan::s_singletonInstance = this;
}

FileMan::FileMan(const QString &appName, QObject *parent)
    : FileMan{ FileManPrivate::resolveDataDir(appName), parent }
{
}

FileMan::FileMan(const QString &orgName, const QString &appName, QObject *parent)
    : FileMan{ FileManPrivate::resolveDataDir(orgName, appName), parent }
{
}

FileMan::FileMan(QObject *parent) : FileMan{ FileManPrivate::resolveDataDir(), parent } { }
FileMan::~FileMan() = default;

auto FileMan::instance() -> FileMan &
{
    if (!FileMan::instanceExists())
        logFatal << "No instance constructed. FileMan must be explicitly instantiated before "
                    "invoking the instance";

    return *FileMan::s_singletonInstance;
}

auto FileMan::instanceExists() -> bool
{
    return (FileMan::s_singletonInstance != nullptr);
}