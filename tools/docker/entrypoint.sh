#!/bin/bash

if [ ! -d "${HOME}/.minemon/" ]; then
    mkdir -p ${HOME}/.minemon/
fi

if [ ! -f "${HOME}/.minemon/minemon.conf" ]; then
    cp /minemon.conf ${HOME}/.minemon/minemon.conf
fi

exec "$@"
