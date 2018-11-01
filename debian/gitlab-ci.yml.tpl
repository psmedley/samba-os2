include: https://salsa.debian.org/salsa-ci-team/pipeline/raw/master/salsa-ci.yml

# end of salsa pipeline bot parser
before_script:
  - echo 'deb http://deb.debian.org/debian experimental main' > /etc/apt/sources.list.d/experimental.list
  - "echo 'Package: ldb-tools libldb* python*-ldb*\nPin: release a=experimental\nPin-Priority: 500' > /etc/apt/preferences.d/experimental.pref"
