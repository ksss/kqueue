require "bundler/gem_tasks"
require 'rake/extensiontask'
require 'rake/testtask'

task :default => [:compile, :test]

Rake::ExtensionTask.new('kqueue') do |ext|
end
Rake::TestTask.new {|t| t.libs << 'test'}
