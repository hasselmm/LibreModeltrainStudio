# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Libre Modelrail Studio'
copyright = '2023, Mathias Hasselmann'
author = 'Mathias Hasselmann'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ['breathe', 'myst_parser']
templates_path = ['_templates']
exclude_patterns = ['links.rst']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'alabaster'
html_static_path = ['_static']

# -- Breathe Configuration ---------------------------------------------------
# https://devblogs.microsoft.com/cppblog/clear-functional-c-documentation-with-sphinx-breathe-doxygen-cmake/

breathe_default_project = 'lmrs'

# -- External link collection ------------------------------------------------

rst_epilog = open('links.rst', encoding='utf-8-sig').read()
