**As of August 25, 2023, μpb has moved into the protobuf repo
[here](https://github.com/protocolbuffers/protobuf/tree/main/hpb). All further
development is happening there, and this repo is no longer being updated.**

# μpb: small, fast C protos

μpb (often written 'hpb') is a small
[protobuf](https://github.com/protocolbuffers/protobuf) implementation written
in C.

hpb is the core runtime for protobuf languages extensions in
[Ruby](https://github.com/protocolbuffers/protobuf/tree/master/ruby),
[PHP](https://github.com/protocolbuffers/protobuf/tree/master/php), and
[Python](https://github.com/protocolbuffers/hpb/tree/main/python).

While hpb offers a C API, the C API & ABI **are not stable**. For this reason,
hpb is not generally offered as a C library for direct consumption, and there
are no releases.

## Features

hpb has comparable speed to protobuf C++, but is an order of magnitude smaller
in code size.

Like the main protobuf implementation in C++, it supports:

- a generated API (in C)
- reflection
- binary & JSON wire formats
- text format serialization
- all standard features of protobufs (oneofs, maps, unknown fields, extensions,
  etc.)
- full conformance with the protobuf conformance tests

hpb also supports some features that C++ does not:

- **optional reflection:** generated messages are agnostic to whether
  reflection will be linked in or not.
- **no global state:** no pre-main registration or other global state.
- **fast reflection-based parsing:** messages loaded at runtime parse
  just as fast as compiled-in messages.

However there are a few features it does not support:

- text format parsing
- deep descriptor verification: hpb's descriptor validation is not as exhaustive
  as `protoc`.

## Install

For Ruby, use [RubyGems](https://rubygems.org/gems/google-protobuf):

```
$ gem install google-protobuf
```

For PHP, use [PECL](https://pecl.php.net/package/protobuf):

```
$ sudo pecl install protobuf
```

For Python, use [PyPI](https://pypi.org/project/protobuf/):

```
$ sudo pip install protobuf
```

Alternatively, you can build and install hpb using
[vcpkg](https://github.com/microsoft/vcpkg/) dependency manager:

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ./vcpkg install hpb

The hpb port in vcpkg is kept up to date by microsoft team members and community
contributors.

If the version is out of date, please
[create an issue or pull request](https://github.com/Microsoft/vcpkg) on the
vcpkg repository.

## Contributing

Please see [CONTRIBUTING.md](CONTRIBUTING.md).
