Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/trusty64"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  config.vm.synced_folder "./", "/home/vagrant/project"

  config.vm.provision "shell", inline: <<-SHELL
    apt-get update
    bash <(curl -fsSL https://xmake.io/shget.text)
  SHELL
end
