#include <fileman_p.hpp>

Q_LOGGING_CATEGORY(fileman, "FileMan")

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

//
// Opaque/PIMPL structs add_file_t and rm_file_t for maintaining ABI compatiblity
//

// Empty constructors
add_file_t::add_file_t() : d_ptr{ new AddFileType } { }
rm_file_t::rm_file_t() : d_ptr{ new RmFileType } { }

// Constructors having references to members
add_file_t::add_file_t(int status, const QUrl &location, const QString &id)
    : d_ptr{ new AddFileType }
{
    this->d_ptr->status   = status;
    this->d_ptr->location = location;
    this->d_ptr->id       = id;
}

rm_file_t::rm_file_t(int status_l, int status_b) : d_ptr{ new RmFileType }
{
    this->d_ptr->status_l = status_l;
    this->d_ptr->status_b = status_b;
}

// Copy/Move constructors
add_file_t::add_file_t(const add_file_t &)     = default;
add_file_t::add_file_t(add_file_t &&) noexcept = default;
rm_file_t::rm_file_t(const rm_file_t &)        = default;
rm_file_t::rm_file_t(rm_file_t &&) noexcept    = default;

auto add_file_t::operator=(const add_file_t &) -> add_file_t &     = default;
auto add_file_t::operator=(add_file_t &&) noexcept -> add_file_t & = default;
auto rm_file_t::operator=(const rm_file_t &) -> rm_file_t &        = default;
auto rm_file_t::operator=(rm_file_t &&) noexcept -> rm_file_t &    = default;

// Destructors
add_file_t::~add_file_t() = default;
rm_file_t::~rm_file_t()   = default;

// Accesors
auto add_file_t::status() const -> int
{
    return (this->d_ptr == nullptr) ? -1 : this->d_ptr->status;
}

auto add_file_t::location() const -> QUrl
{
    return (this->d_ptr == nullptr) ? QUrl{ } : this->d_ptr->location;
}

auto add_file_t::id() const -> QString
{
    return (this->d_ptr == nullptr) ? QString{ } : this->d_ptr->id;
}

// True only after this object has been moved-from. A default-constructed add_file_t is never null.
auto add_file_t::isNull() const noexcept -> bool
{
    return (this->d_ptr != nullptr);
}

auto rm_file_t::status_b() const -> int
{
    return (this->d_ptr == nullptr) ? -1 : this->d_ptr->status_b;
}

auto rm_file_t::status_l() const -> int
{
    return (this->d_ptr == nullptr) ? -1 : this->d_ptr->status_l;
}

auto rm_file_t::isNull() const noexcept -> bool
{
    return this->d_ptr != nullptr;
}