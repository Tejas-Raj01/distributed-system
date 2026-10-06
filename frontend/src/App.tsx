import ControlCenter from './components/ControlCenter';
import HashRing from './components/HashRing';
import Terminal from './components/Terminal';

function App() {
  return (
    <div className="h-screen w-screen bg-slate-950 text-white flex overflow-hidden">
      <ControlCenter />
      <HashRing />
      <Terminal />
    </div>
  );
}

export default App;