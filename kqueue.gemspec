Gem::Specification.new do |spec|
  spec.name          = "kqueue"
  spec.version       = "0.0.1"
  spec.authors       = ["ksss"]
  spec.email         = ["co000ri@gmail.com"]
  spec.summary       = %q{A Ruby binding for kqueue(2)}
  spec.description   = %q{A Ruby binding for kqueue(2)}
  spec.homepage      = "https://github.com/ksss/kqueue"
  spec.license       = "MIT"

  spec.files         = `git ls-files`.split($/)
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/kqueue/extconf.rb"]

  spec.add_development_dependency "bundler"
  spec.add_development_dependency "rake"
  spec.add_development_dependency 'rake-compiler'
  spec.add_development_dependency 'test-unit'
end
