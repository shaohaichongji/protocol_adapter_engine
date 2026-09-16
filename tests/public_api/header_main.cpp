int HeaderExportCompiles();
int HeaderVersionCompiles();
int HeaderStreamFramerCompiles();
int HeaderDescriptionCompiles();
int HeaderCompilerCompiles();
int HeaderCodecCompiles();

int main() {
  return HeaderExportCompiles() + HeaderVersionCompiles() + HeaderDescriptionCompiles() +
         HeaderCompilerCompiles() + HeaderCodecCompiles() + HeaderStreamFramerCompiles();
}
