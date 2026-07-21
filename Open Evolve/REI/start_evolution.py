import sys
import pathlib
import asyncio

BASE_DIR = pathlib.Path(__file__).parent.resolve()
sys.path.append(str(BASE_DIR.parent))

from openevolve import Config, OpenEvolve
from code_to_query import format_query_code

async def main():
    initial_program_dir = BASE_DIR / "code"
    openevolve_output_dir = BASE_DIR / "openevolve_output"
    openevolve_output_dir.mkdir(parents=True, exist_ok=True)

    initial_program_path = openevolve_output_dir / "initial_program.txt"
    initial_program_path.write_text(format_query_code(str(initial_program_dir)), encoding='utf-8')

    config = Config()
    config.max_iterations = 100
    config.evaluator.cascade_evaluation = False
    config.evaluator.timeout = 360 
    config.database.num_islands = 5
    config.database.population_size = 1000

    config.llm.temperature = 1.0
    config.llm.max_tokens = 16000
    config.llm.timeout = 600 # give the llm time to think
    config.llm.rebuild_models()

    # Remove number of lines limit and code length limits
    config.language = "mixed"
    config.max_code_length = 50 * 3000
    config.prompt.diff_summary_max_lines = 3000
    config.prompt.diff_summary_max_line_len = 3000
    
    # We have two options in our case: the first is to instruct the LLM to output where it needed to edit, 
    # this also means that the LLM should take the whole program as an input
    # The second which what we are using here, is to let the LLM output the while file that it took an an input
    # this will give the LLM the ability to do big changes
    # To switch to the first option you need to set this option to True and uncomment the prompt bellow that instruct
    # the LLM to use Search and Replace, and edit the file code_to_query.py so it pack the whole program
    config.diff_based_evolution = False
    config.prompt.programs_as_changes_description = False
    
    # config.prompt.system_message = """You are an expert algorithm engineer.
    # You MUST return your code changes using the SEARCH/REPLACE format. DO NOT write any preamble or explanations.
    # You MUST wrap your changes for each file using the exact format:
    # * filename *:
    # @@@
    # <<<<<<< SEARCH
    # [exact old code]
    # =======
    # [new code]
    # >>>>>>> REPLACE
    # @@@

    # Failure to include the `* filename *:` and `@@@` delimiters will cause a parsing error.
    # """

    config.prompt.template_variations = {
        "improvement_suggestion" : [
            "try to come up with a new heuristics",
            "is the normal distribution good for this case?",
            "should the sampling function stay the same for every benchmark? (i.e we could change it with IC size)"]
        }

    evolve = OpenEvolve(
        initial_program_path=str(initial_program_path),
        evaluation_file=str(BASE_DIR / "evaluator.py"),
        config=config,
        output_dir=str(openevolve_output_dir)
    )

    print("Starting evolution for REI...")
    best_program = await evolve.run()
    
    print("\n=== Evolution Complete ===")
    print("Best Score:", best_program.metrics.get("combined_score", 0))
    print("Metrics:", best_program.metrics)

if __name__ == "__main__":
    asyncio.run(main())
